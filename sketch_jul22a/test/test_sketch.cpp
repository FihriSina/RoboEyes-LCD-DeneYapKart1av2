// Host-based unit tests for the RoboEyes LCD sketch.
//
// The sketch is a single Arduino translation unit, so we #include it directly
// (after the mock Arduino runtime has been force-included by the build) and
// exercise its pure logic. setup()/loop() are defined by the sketch but never
// invoked here.
//
// Build/run: see test/run_tests.sh (or the Makefile).

#include "test_framework.h"

// Pull in the sketch under test. All of its functions and globals become
// visible to the tests below.
#include "../sketch_jul22a.ino"

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Build a Desen from eight 5-bit rows.
static Desen mkDesen(uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4,
                     uint8_t r5, uint8_t r6, uint8_t r7) {
  Desen d;
  d.satir[0] = r0; d.satir[1] = r1; d.satir[2] = r2; d.satir[3] = r3;
  d.satir[4] = r4; d.satir[5] = r5; d.satir[6] = r6; d.satir[7] = r7;
  return d;
}

static Desen fullDesen() {
  return mkDesen(B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111);
}
static Desen emptyDesen() {
  return mkDesen(0, 0, 0, 0, 0, 0, 0, 0);
}

// Reset the sketch's mutable global animation state to known values so tests
// do not leak into each other.
static void resetState() {
  mock_arduino::g_millis = 0;
  randomSeed(12345);

  aktifIfade = NORMAL;
  gozAcikligi = 100;
  kirpmaDurumu = KIRPMA_BEKLEME;
  kalanKirpma = 0;
  kirpmaAsamasiBaslangici = 0;
  sonrakiKirpma = 0;
  sonrakiBakis = 0;
  sonrakiIfade = 0;
  sonrakiKare = 0;
  bakisX100 = 0;
  bakisY100 = 0;
  hedefBakisX = 0;
  hedefBakisY = 0;
  ozelAnimasyon = OZEL_YOK;
  ozelAnimasyonBaslangici = 0;
  ozelAnimasyonBitisi = 0;
  aktifSahne = SAHNE_GOZLER;
  yumrukSahnesiBaslangici = 0;

  cgramHazir = false;
  tekilDesenSayisi = 0;
  memset(aktifCgram, 0, sizeof(aktifCgram));
  memset(eskiHucreHaritasi, 254, sizeof(eskiHucreHaritasi));

  Serial.reset();
  Wire.endTransmissionResult = 0;
  lcd.resetLog();
}

// Count filled pixels in the canvas.
static int canvasPixelCount() {
  int n = 0;
  for (int y = 0; y < PIKSEL_YUKSEKLIK; y++)
    for (int x = 0; x < PIKSEL_GENISLIK; x++)
      if (pikselTamponu[y][x]) n++;
  return n;
}

// ---------------------------------------------------------------------------
// Module: small numeric helpers
// ---------------------------------------------------------------------------

TEST(helpers, sinirla16_clamps_both_ends) {
  CHECK_EQ(sinirla16(5, 0, 10), 5);
  CHECK_EQ(sinirla16(-4, 0, 10), 0);
  CHECK_EQ(sinirla16(20, 0, 10), 10);
  CHECK_EQ(sinirla16(0, 0, 10), 0);
  CHECK_EQ(sinirla16(10, 0, 10), 10);
  // Negative range.
  CHECK_EQ(sinirla16(-50, -20, -5), -20);
  CHECK_EQ(sinirla16(0, -20, -5), -5);
}

TEST(helpers, ilerleme1000_maps_time_to_permille) {
  CHECK_EQ(ilerleme1000(0, 0, 1000), 0);        // before/at start
  CHECK_EQ(ilerleme1000(1000, 0, 1000), 1000);  // at end -> saturated
  CHECK_EQ(ilerleme1000(2000, 0, 1000), 1000);  // past end -> saturated
  CHECK_EQ(ilerleme1000(500, 0, 1000), 500);    // midpoint
  // Non-zero start offset.
  CHECK_EQ(ilerleme1000(100, 100, 300), 0);
  CHECK_EQ(ilerleme1000(200, 100, 300), 500);
  CHECK_EQ(ilerleme1000(300, 100, 300), 1000);
}

TEST(helpers, smoothstep_endpoints_and_midpoint) {
  CHECK_EQ(yumusakIlerleme1000(0), 0);
  CHECK_EQ(yumusakIlerleme1000(1000), 1000);
  // smoothstep(0.5) == 0.5
  CHECK_EQ(yumusakIlerleme1000(500), 500);
  // Monotonic and eased: value at 0.25 is below linear (250).
  CHECK(yumusakIlerleme1000(250) < 250);
  CHECK(yumusakIlerleme1000(750) > 750);
}

TEST(helpers, easeout_endpoints_and_shape) {
  CHECK_EQ(hizliBasla1000(0), 0);
  CHECK_EQ(hizliBasla1000(1000), 1000);
  // Ease-out starts fast: value at 0.25 is above linear.
  CHECK(hizliBasla1000(250) > 250);
  CHECK(hizliBasla1000(500) > 500);
}

TEST(helpers, bit_population_count) {
  CHECK_EQ(bitSayisi(0), 0);
  CHECK_EQ(bitSayisi(B11111), 5);
  CHECK_EQ(bitSayisi(0xFF), 8);
  CHECK_EQ(bitSayisi(B10000), 1);
  CHECK_EQ(bitSayisi(0b10101), 3);
}

// ---------------------------------------------------------------------------
// Module: pattern (Desen) operations
// ---------------------------------------------------------------------------

TEST(pattern, equality) {
  CHECK(desenlerAyni(fullDesen(), fullDesen()));
  CHECK(desenlerAyni(emptyDesen(), emptyDesen()));
  CHECK(!desenlerAyni(fullDesen(), emptyDesen()));
  Desen a = mkDesen(1, 2, 3, 4, 5, 6, 7, 8);
  Desen b = a;
  b.satir[4] = 31;
  CHECK(!desenlerAyni(a, b));
}

TEST(pattern, empty_and_full_detection) {
  CHECK(desenBos(emptyDesen()));
  CHECK(!desenBos(fullDesen()));
  CHECK(desenDolu(fullDesen()));
  CHECK(!desenDolu(emptyDesen()));

  // Only the low 5 bits count: high bits set but low bits clear -> still empty.
  Desen highBits = mkDesen(0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0);
  CHECK(desenBos(highBits));
  // Low 5 bits all set (with stray high bits) -> still "full".
  Desen fullWithHigh = mkDesen(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
  CHECK(desenDolu(fullWithHigh));
}

TEST(pattern, hamming_distance) {
  CHECK_EQ(desenUzakligi(emptyDesen(), emptyDesen()), 0);
  // Every row differs by all 5 bits -> 40.
  CHECK_EQ(desenUzakligi(emptyDesen(), fullDesen()), 40);
  // Single differing bit.
  Desen one = mkDesen(B10000, 0, 0, 0, 0, 0, 0, 0);
  CHECK_EQ(desenUzakligi(emptyDesen(), one), 1);
  // Distance ignores bits above the low 5.
  Desen stray = mkDesen(0xE0, 0, 0, 0, 0, 0, 0, 0);
  CHECK_EQ(desenUzakligi(emptyDesen(), stray), 0);
}

TEST(pattern, display_error_penalizes_excess_more) {
  // Identical -> zero error.
  CHECK_EQ(desenGosterimHatasi(fullDesen(), fullDesen()), 0);

  Desen target = mkDesen(B10000, 0, 0, 0, 0, 0, 0, 0);
  Desen missing = emptyDesen();          // target pixel not shown -> 1 missing
  Desen extra = mkDesen(B11000, 0, 0, 0, 0, 0, 0, 0);  // one stray extra pixel

  const uint8_t missErr = desenGosterimHatasi(target, missing);
  const uint8_t extraErr = desenGosterimHatasi(target, extra);
  CHECK_EQ(missErr, 1);    // one missing pixel = weight 1
  CHECK_EQ(extraErr, 5);   // one excess pixel = weight 5
  CHECK(extraErr > missErr);
}

// ---------------------------------------------------------------------------
// Module: 100x32 canvas drawing engine
// ---------------------------------------------------------------------------

TEST(canvas, clear_zeros_buffer) {
  doluDikdortgenCiz(0, 0, PIKSEL_GENISLIK, PIKSEL_YUKSEKLIK, true);
  CHECK(canvasPixelCount() > 0);
  tuvaliTemizle();
  CHECK_EQ(canvasPixelCount(), 0);
}

TEST(canvas, pixel_write_respects_bounds) {
  tuvaliTemizle();
  pikselYaz(-1, 5, true);
  pikselYaz(PIKSEL_GENISLIK, 5, true);
  pikselYaz(5, -1, true);
  pikselYaz(5, PIKSEL_YUKSEKLIK, true);
  CHECK_EQ(canvasPixelCount(), 0);  // all out of bounds, ignored

  pikselYaz(10, 10, true);
  CHECK_EQ(pikselTamponu[10][10], 1);
  pikselYaz(10, 10, false);
  CHECK_EQ(pikselTamponu[10][10], 0);
}

TEST(canvas, filled_rectangle_area) {
  tuvaliTemizle();
  doluDikdortgenCiz(3, 4, 10, 6, true);
  CHECK_EQ(canvasPixelCount(), 10 * 6);
  CHECK_EQ(pikselTamponu[4][3], 1);
  CHECK_EQ(pikselTamponu[9][12], 1);
  CHECK_EQ(pikselTamponu[4][13], 0);  // just past right edge
  CHECK_EQ(pikselTamponu[10][3], 0);  // just past bottom edge
}

TEST(canvas, rectangle_clipped_at_edges) {
  tuvaliTemizle();
  // Rectangle straddling the top-left corner; only the in-bounds part counts.
  doluDikdortgenCiz(-2, -2, 5, 5, true);
  CHECK_EQ(canvasPixelCount(), 3 * 3);
}

TEST(canvas, ellipse_symmetry_and_center) {
  tuvaliTemizle();
  doluElipsCiz(50, 16, 6, 4, true);
  CHECK_EQ(pikselTamponu[16][50], 1);  // center filled
  // Horizontal/vertical symmetry about the center.
  for (int dx = -6; dx <= 6; dx++) {
    CHECK_EQ(pikselTamponu[16][50 + dx], pikselTamponu[16][50 - dx]);
  }
  for (int dy = -4; dy <= 4; dy++) {
    CHECK_EQ(pikselTamponu[16 + dy][50], pikselTamponu[16 - dy][50]);
  }
}

TEST(canvas, line_horizontal_and_diagonal) {
  tuvaliTemizle();
  cizgiCiz(2, 5, 8, 5, true);  // horizontal
  for (int x = 2; x <= 8; x++) CHECK_EQ(pikselTamponu[5][x], 1);
  CHECK_EQ(canvasPixelCount(), 7);

  tuvaliTemizle();
  cizgiCiz(0, 0, 4, 4, true);  // 45-degree diagonal -> 5 pixels
  CHECK_EQ(canvasPixelCount(), 5);
  for (int i = 0; i <= 4; i++) CHECK_EQ(pikselTamponu[i][i], 1);
}

TEST(canvas, line_can_erase) {
  tuvaliTemizle();
  doluDikdortgenCiz(0, 0, 10, 3, true);
  cizgiCiz(0, 1, 9, 1, false);  // erase the middle row
  for (int x = 0; x < 10; x++) CHECK_EQ(pikselTamponu[1][x], 0);
  CHECK_EQ(pikselTamponu[0][0], 1);  // other rows intact
}

// ---------------------------------------------------------------------------
// Module: canvas -> 5x8 cell extraction
// ---------------------------------------------------------------------------

TEST(cells, extraction_maps_pixels_to_bits) {
  resetState();
  tuvaliTemizle();
  // Fill the very first pixel (top-left) -> cell 0, row 0, MSB (B10000).
  pikselYaz(0, 0, true);
  hucreDesenleriniCikar();
  CHECK_EQ(hucreDesenleri[0].satir[0], B10000);
  // All other rows of cell 0 stay empty.
  for (int y = 1; y < HUCRE_YUKSEKLIK; y++)
    CHECK_EQ(hucreDesenleri[0].satir[y], 0);

  tuvaliTemizle();
  // Fill an entire cell (cell at col 1,row 0 -> pixels x:5..9, y:0..7).
  doluDikdortgenCiz(5, 0, 5, 8, true);
  hucreDesenleriniCikar();
  CHECK(desenDolu(hucreDesenleri[1]));
  CHECK(desenBos(hucreDesenleri[0]));
}

TEST(cells, unique_pattern_dedup_and_skip_trivial) {
  resetState();
  tuvaliTemizle();
  // Two identical partial cells and the rest empty/full-> only 1 unique.
  // Cell 0: a single pixel; Cell 1: same single pixel pattern.
  pikselYaz(0, 0, true);   // cell 0
  pikselYaz(5, 0, true);   // cell 1 (same local pattern)
  // Fill cell 2 fully (should be skipped as "full").
  doluDikdortgenCiz(10, 0, 5, 8, true);
  hucreDesenleriniCikar();
  tekilDesenleriBul();
  CHECK_EQ(tekilDesenSayisi, 1);
  CHECK_EQ(tekilDesenler[0].tekrar, 2);
}

// ---------------------------------------------------------------------------
// Module: dynamic 8-pattern compression
// ---------------------------------------------------------------------------

TEST(compress, nearest_cgram_picks_lowest_error) {
  Desen protos[OZEL_DESEN_SAYISI];
  for (int k = 0; k < OZEL_DESEN_SAYISI; k++) protos[k] = emptyDesen();
  protos[3] = fullDesen();
  // A full target should map to the full prototype (slot 3).
  CHECK_EQ(enYakinCgram(fullDesen(), protos), 3);
  // An empty target maps to an empty prototype (slot 0, first match).
  CHECK_EQ(enYakinCgram(emptyDesen(), protos), 0);
}

TEST(compress, few_patterns_placed_directly) {
  resetState();
  // Three distinct partial patterns -> should all appear among the 8 protos.
  tekilDesenSayisi = 3;
  tekilDesenler[0] = {mkDesen(B10000, 0, 0, 0, 0, 0, 0, 0), 5};
  tekilDesenler[1] = {mkDesen(0, 0, 0, 0, 0, 0, 0, B00001), 3};
  tekilDesenler[2] = {mkDesen(B11111, 0, 0, 0, 0, 0, 0, 0), 1};

  Desen out[OZEL_DESEN_SAYISI];
  enIyiSekizDeseniOlustur(out);

  for (int i = 0; i < 3; i++) {
    bool found = false;
    for (int k = 0; k < OZEL_DESEN_SAYISI; k++)
      if (desenlerAyni(out[k], tekilDesenler[i].desen)) found = true;
    CHECK(found);
  }
}

TEST(compress, many_patterns_reduced_to_eight) {
  resetState();
  // 20 distinct patterns must be represented by exactly 8 prototypes, and each
  // input pattern must map to some prototype (clustering is well-formed).
  tekilDesenSayisi = 20;
  for (int i = 0; i < 20; i++) {
    // Vary the pattern deterministically across rows.
    tekilDesenler[i] = {mkDesen((i * 1) & B11111, (i * 3) & B11111,
                                (i * 5) & B11111, (i * 7) & B11111,
                                (i * 11) & B11111, (i * 13) & B11111,
                                (i * 2) & B11111, (i * 6) & B11111),
                        static_cast<uint8_t>(1 + (i % 4))};
  }
  Desen out[OZEL_DESEN_SAYISI];
  enIyiSekizDeseniOlustur(out);

  // Every input maps to a prototype with finite error (sanity of clustering).
  for (int i = 0; i < 20; i++) {
    const uint8_t slot = enYakinCgram(tekilDesenler[i].desen, out);
    CHECK(slot < OZEL_DESEN_SAYISI);
  }
}

TEST(compress, uniform_cgram_detection) {
  resetState();
  for (int k = 0; k < OZEL_DESEN_SAYISI; k++) aktifCgram[k] = emptyDesen();
  CHECK(aktifCgramTekTipMi());
  aktifCgram[4] = fullDesen();
  CHECK(!aktifCgramTekTipMi());
}

TEST(compress, cell_map_uses_blank_and_full_shortcuts) {
  resetState();
  tuvaliTemizle();
  // Cell 0 empty, cell 1 full, cell 2 a partial pattern.
  doluDikdortgenCiz(5, 0, 5, 8, true);   // cell 1 full
  pikselYaz(10, 0, true);                // cell 2 single pixel
  hucreDesenleriniCikar();

  Desen protos[OZEL_DESEN_SAYISI];
  for (int k = 0; k < OZEL_DESEN_SAYISI; k++) protos[k] = emptyDesen();
  protos[2] = mkDesen(B10000, 0, 0, 0, 0, 0, 0, 0);

  hucreHaritasiniOlustur(protos);
  CHECK_EQ(yeniHucreHaritasi[0], BOS_KARAKTER);
  CHECK_EQ(yeniHucreHaritasi[1], TAM_BLOK_KARAKTER);
  CHECK(yeniHucreHaritasi[2] < OZEL_DESEN_SAYISI);  // a custom slot index
}

// ---------------------------------------------------------------------------
// Module: display write pipeline
// ---------------------------------------------------------------------------

TEST(render, only_changed_cells_written) {
  resetState();
  // Blank frame: new map all spaces, old map all 254 (init sentinel) -> every
  // cell differs, so every cell is written once.
  for (int h = 0; h < HUCRE_SAYISI; h++) yeniHucreHaritasi[h] = BOS_KARAKTER;
  lcd.resetLog();
  hucreHaritasiniEkranaYaz();
  CHECK_EQ((int)lcd.writeCalls.size(), HUCRE_SAYISI);

  // Second write with an identical map -> nothing changed, no writes.
  lcd.resetLog();
  hucreHaritasiniEkranaYaz();
  CHECK_EQ((int)lcd.writeCalls.size(), 0);

  // Change a single cell -> exactly one write.
  yeniHucreHaritasi[42] = TAM_BLOK_KARAKTER;
  lcd.resetLog();
  hucreHaritasiniEkranaYaz();
  CHECK_EQ((int)lcd.writeCalls.size(), 1);
  CHECK_EQ(lcd.writeCalls[0].value, TAM_BLOK_KARAKTER);
}

TEST(render, full_pipeline_programs_cgram) {
  resetState();
  tuvaliTemizle();
  aktifIfade = NORMAL;
  gozleriTuvaleCiz(0);   // draw a real eye frame
  lcd.resetLog();
  kareyiEkranaGonder();  // extract -> compress -> program CGRAM -> write cells
  CHECK(cgramHazir);
  // The first detailed frame programs all 8 custom characters.
  CHECK_EQ((int)lcd.createCharCalls.size(), OZEL_DESEN_SAYISI);
  // Something was drawn to the screen.
  CHECK(lcd.writeCalls.size() > 0);
}

// ---------------------------------------------------------------------------
// Module: blink state machine
// ---------------------------------------------------------------------------

TEST(blink, start_only_from_idle) {
  resetState();
  kirpmaDurumu = KIRPMA_BEKLEME;
  kirpmayiBaslat(100);
  CHECK_EQ(kirpmaDurumu, KIRPMA_KAPANIYOR);
  CHECK(kalanKirpma >= 1);

  // Calling again while not idle is a no-op.
  const KirpmaDurumu before = kirpmaDurumu;
  kirpmayiBaslat(200);
  CHECK_EQ(kirpmaDurumu, before);
}

TEST(blink, full_cycle_closes_and_reopens) {
  resetState();
  kirpmayiBaslat(0);
  CHECK_EQ(kirpmaDurumu, KIRPMA_KAPANIYOR);

  // After the closing phase the eye is shut.
  mock_arduino::g_millis = 130;
  kirpmayiGuncelle(130);
  CHECK_EQ(kirpmaDurumu, KIRPMA_KAPALI);
  CHECK_EQ(gozAcikligi, 0);

  // Hold closed, then start opening.
  kirpmayiGuncelle(130 + 70);
  CHECK_EQ(kirpmaDurumu, KIRPMA_ACILIYOR);

  // Finish opening -> back to idle, eye fully open (single blink).
  kirpmayiGuncelle(130 + 70 + 180);
  CHECK_EQ(kirpmaDurumu, KIRPMA_BEKLEME);
  CHECK_EQ(gozAcikligi, 100);
}

TEST(blink, closing_progress_monotonic) {
  resetState();
  kirpmayiBaslat(0);
  uint8_t prev = 101;
  for (unsigned long t = 0; t < 125; t += 25) {
    kirpmaDurumu = KIRPMA_KAPANIYOR;
    kirpmaAsamasiBaslangici = 0;
    kirpmayiGuncelle(t);
    CHECK(gozAcikligi <= prev);  // never re-opens while closing
    prev = gozAcikligi;
  }
}

// ---------------------------------------------------------------------------
// Module: gaze low-pass movement
// ---------------------------------------------------------------------------

TEST(gaze, approaches_target_and_snaps) {
  resetState();
  // Disable auto-retarget influence by setting sonrakiBakis far in the future.
  sonrakiBakis = 1000000;
  hedefBakisX = 5;   // target 500 (x100)
  hedefBakisY = -3;  // target -300
  bakisX100 = 0;
  bakisY100 = 0;

  int16_t prevX = bakisX100;
  for (int i = 0; i < 40; i++) {
    bakisiGuncelle(0);
    CHECK(bakisX100 >= prevX);  // moves toward positive target, never overshoots down
    prevX = bakisX100;
  }
  // Eventually snaps exactly onto the target.
  CHECK_EQ(bakisX100, (int16_t)500);
  CHECK_EQ(bakisY100, (int16_t)-300);
}

// ---------------------------------------------------------------------------
// Module: special animation trigger
// ---------------------------------------------------------------------------

TEST(special, start_sets_window) {
  resetState();
  ozelAnimasyonuBaslat(OZEL_SASIRMA, 1000);
  CHECK_EQ(ozelAnimasyon, OZEL_SASIRMA);
  CHECK_EQ(ozelAnimasyonBaslangici, 1000UL);
  CHECK_EQ(ozelAnimasyonBitisi, 1900UL);  // 900 ms window

  ozelAnimasyonuBaslat(OZEL_GERILIM, 2000);
  CHECK_EQ(ozelAnimasyonBitisi, 3300UL);  // 1300 ms window for gerilim
}

// ---------------------------------------------------------------------------
// Module: serial command parsing
// ---------------------------------------------------------------------------

TEST(serial, digit_commands_select_expression) {
  resetState();
  Serial.feed("1");
  seriKomutlariOku(0);
  CHECK_EQ(aktifIfade, NORMAL);

  Serial.feed("5");
  seriKomutlariOku(0);
  CHECK_EQ(aktifIfade, KRUPIYE);

  Serial.feed("8");
  seriKomutlariOku(0);
  CHECK_EQ(aktifIfade, CILGIN);
}

TEST(serial, letter_commands_are_case_insensitive) {
  resetState();
  Serial.feed("b");
  seriKomutlariOku(0);
  CHECK_EQ(kirpmaDurumu, KIRPMA_KAPANIYOR);  // blink started

  resetState();
  Serial.feed("H");
  seriKomutlariOku(0);
  CHECK_EQ(ozelAnimasyon, OZEL_DARBE);

  resetState();
  Serial.feed("t");
  seriKomutlariOku(0);
  CHECK_EQ(ozelAnimasyon, OZEL_GERILIM);
}

TEST(serial, punch_command_and_legacy_alias) {
  resetState();
  Serial.feed("Y");
  seriKomutlariOku(500);
  CHECK_EQ(aktifSahne, SAHNE_YUMRUK_GIRISI);
  CHECK_EQ(yumrukSahnesiBaslangici, 500UL);

  // Legacy 'P' triggers the same scene.
  resetState();
  Serial.feed("p");
  seriKomutlariOku(700);
  CHECK_EQ(aktifSahne, SAHNE_YUMRUK_GIRISI);
}

TEST(serial, unknown_command_is_ignored) {
  resetState();
  aktifIfade = MUTLU;
  Serial.feed("Z");
  seriKomutlariOku(0);
  CHECK_EQ(aktifIfade, MUTLU);  // unchanged
}

TEST(serial, multiple_commands_in_one_read) {
  resetState();
  Serial.feed("13");  // Normal then Yorgun
  seriKomutlariOku(0);
  CHECK_EQ(aktifIfade, YORGUN);
}

// ---------------------------------------------------------------------------
// Module: hardware presence check
// ---------------------------------------------------------------------------

TEST(hardware, lcd_presence_reflects_i2c_ack) {
  resetState();
  Wire.endTransmissionResult = 0;   // ACK
  CHECK(lcdBagliMi());
  CHECK_EQ(Wire.lastAddress, LCD_ADRESI);

  Wire.endTransmissionResult = 2;   // NACK / no device
  CHECK(!lcdBagliMi());
}

// ---------------------------------------------------------------------------
// Module: punch scene timeline
// ---------------------------------------------------------------------------

TEST(punch, start_resets_eye_state) {
  resetState();
  gozAcikligi = 10;
  bakisX100 = 400;
  ozelAnimasyon = OZEL_DARBE;
  kalanKirpma = 2;
  yumrukSahnesiniBaslat(1000);
  CHECK_EQ(aktifSahne, SAHNE_YUMRUK_GIRISI);
  CHECK_EQ(gozAcikligi, 100);
  CHECK_EQ(bakisX100, (int16_t)0);
  CHECK_EQ(ozelAnimasyon, OZEL_YOK);
  CHECK_EQ(kalanKirpma, 0);
}

TEST(punch, draws_during_scene_and_ends_after_timeout) {
  resetState();
  yumrukSahnesiniBaslat(0);
  tuvaliTemizle();
  // Mid-scene: something is drawn to the canvas.
  yumrukSahnesiniTuvaleCiz(400);
  CHECK(canvasPixelCount() > 0);
  CHECK_EQ(aktifSahne, SAHNE_YUMRUK_GIRISI);

  // Past the end of the timeline -> scene hands back to the eyes.
  yumrukSahnesiniTuvaleCiz(YUMRUK_SAHNESI_BITISI + 10);
  CHECK_EQ(aktifSahne, SAHNE_GOZLER);
}

int main() {
  return roboeyes_test::run_all();
}
