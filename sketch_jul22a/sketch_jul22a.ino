/*
  RoboEyes LCD 20x4 - Deneyap Kart 1A v2

  20x4 karakter LCD icin "sanal piksel + dinamik desen sikistirma" motoru.

  Ekranda fiziksel olarak:
    20 x 4 hucre x 5 x 8 nokta = 100 x 32 = 3200 nokta bulunur.

  HD44780 denetleyicisi bu 3200 noktayi bagimsiz bir framebuffer gibi sunmaz;
  ayni anda yalnizca 8 farkli 5x8 ozel desen saklar. Bu kod siniri asmak icin:

    1. Gozleri once RAM'de 100x32 piksel olarak cizer.
    2. Goruntuyu 80 adet 5x8 parcaya boler.
    3. Her karede bu parcalari en uygun 8 desene otomatik olarak sikistirir.
    4. CGRAM desenlerini ve 20x4 hucre haritasini dinamik gunceller.

  Sonuc OLED kadar puruzsuz degildir; fakat karakter LCD'nin gercek donanim
  sinirlari icinde tum ekran alanini kullanan, RoboEyes mantigina yakin bir
  animasyon elde edilir.

  Ozellikler:
  - 100x32 sanal piksel cizim alani
  - Dinamik 8-desene sikistirma (k-medoids)
  - Otomatik kirpma ve ara sira cift kirpma
  - Yumusayarak saga, sola, yukariya ve asagiya bakma
  - Normal, kizgin, yorgun ve mutlu goz sekilleri
  - Sasirma ve gulme benzeri tek-seferlik sallanma animasyonlari
  - delay() yok; sensor ve motor kodlariyla birlikte calisabilir

  Gerekli kutuphane:
  - LiquidCrystal I2C (Arduino Library Manager)

  Baglanti:
  - LCD GND -> Deneyap GND
  - LCD VCC -> Deneyap 3V3
  - LCD SDA -> Deneyap SDA
  - LCD SCL -> Deneyap SCL

  LCD 3.3 V ile calismazsa LCD'yi 5 V ile besleyin; SDA ve SCL hatlarinda
  cift yonlu I2C seviye donusturucu kullanin. Deneyap pinlerine dogrudan
  5 V I2C seviyesi uygulamayin.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------
// KULLANICI AYARLARI
// ---------------------------------------------------------------------------

// Goruntu yoksa 0x27 yerine 0x3F deneyin.
constexpr uint8_t LCD_ADRESI = 0x27;

// PCF8574 tabanli kartlar icin 100 kHz en guvenli degerdir.
// Ekraniniz kararli calisiyorsa 400000 deneyerek kare guncellemesini
// hizlandirabilirsiniz.
constexpr uint32_t I2C_HIZI = 100000;

// 10-14 FPS bu LCD icin uygundur. 400 kHz I2C ile 15 denenebilir.
constexpr uint8_t MAKS_KARE_HIZI = 12;

enum Ifade : uint8_t {
  NORMAL,
  KIZGIN,
  YORGUN,
  MUTLU
};

// Otomobil/far tarzi gorunum icin KIZGIN onerilir.
constexpr Ifade BASLANGIC_IFADESI = KIZGIN;

// true olursa ifade 8-15 saniyede bir kendiliginden degisir.
constexpr bool OTOMATIK_IFADE = false;

// RoboEyes tarzi sade, dolu goz icin false. Iris/goz bebegi icin true.
constexpr bool GOZ_BEBEGI_GOSTER = false;

constexpr bool OTOMATIK_BAKIS = true;
constexpr bool OTOMATIK_KIRPMA = true;
constexpr bool CIFT_KIRPMA = true;

// ---------------------------------------------------------------------------
// EKRAN VE SANAL PIKSEL ALANI
// ---------------------------------------------------------------------------

constexpr uint8_t LCD_SUTUN = 20;
constexpr uint8_t LCD_SATIR = 4;
constexpr uint8_t HUCRE_GENISLIK = 5;
constexpr uint8_t HUCRE_YUKSEKLIK = 8;
constexpr uint8_t PIKSEL_GENISLIK = LCD_SUTUN * HUCRE_GENISLIK;   // 100
constexpr uint8_t PIKSEL_YUKSEKLIK = LCD_SATIR * HUCRE_YUKSEKLIK; // 32
constexpr uint8_t HUCRE_SAYISI = LCD_SUTUN * LCD_SATIR;           // 80
constexpr uint8_t OZEL_DESEN_SAYISI = 8;
constexpr uint8_t BOS_KARAKTER = ' ';
constexpr uint8_t TAM_BLOK_KARAKTER = 255;
constexpr uint16_t KARE_SURESI_MS = 1000 / MAKS_KARE_HIZI;

LiquidCrystal_I2C lcd(LCD_ADRESI, LCD_SUTUN, LCD_SATIR);

// ESP32-S3 icin 3200 bayt cok kucuk bir tampondur.
uint8_t pikselTamponu[PIKSEL_YUKSEKLIK][PIKSEL_GENISLIK];

struct Desen {
  uint8_t satir[HUCRE_YUKSEKLIK]; // Her satirin yalnizca alt 5 biti kullanilir.
};

struct TekilDesen {
  Desen desen;
  uint8_t tekrar;
};

Desen hucreDesenleri[HUCRE_SAYISI];
TekilDesen tekilDesenler[HUCRE_SAYISI];
Desen aktifCgram[OZEL_DESEN_SAYISI];
uint8_t yeniHucreHaritasi[HUCRE_SAYISI];
uint8_t eskiHucreHaritasi[HUCRE_SAYISI];

bool cgramHazir = false;
uint8_t tekilDesenSayisi = 0;

// ---------------------------------------------------------------------------
// ANIMASYON DURUMU
// ---------------------------------------------------------------------------

Ifade aktifIfade = BASLANGIC_IFADESI;

int16_t bakisX100 = 0;
int16_t bakisY100 = 0;
int8_t hedefBakisX = 0;
int8_t hedefBakisY = 0;

enum KirpmaDurumu : uint8_t {
  KIRPMA_BEKLEME,
  KIRPMA_KAPANIYOR,
  KIRPMA_KAPALI,
  KIRPMA_ACILIYOR,
  KIRPMA_ARASI
};

KirpmaDurumu kirpmaDurumu = KIRPMA_BEKLEME;
uint8_t gozAcikligi = 100;
uint8_t kalanKirpma = 0;
unsigned long kirpmaAsamasiBaslangici = 0;
unsigned long sonrakiKirpma = 0;
unsigned long sonrakiBakis = 0;
unsigned long sonrakiIfade = 0;
unsigned long sonrakiKare = 0;

enum OzelAnimasyon : uint8_t {
  OZEL_YOK,
  OZEL_SASIRMA,
  OZEL_GULME
};

OzelAnimasyon ozelAnimasyon = OZEL_YOK;
unsigned long ozelAnimasyonBaslangici = 0;
unsigned long ozelAnimasyonBitisi = 0;

// ---------------------------------------------------------------------------
// KUCUK YARDIMCILAR
// ---------------------------------------------------------------------------

int16_t sinirla16(int16_t deger, int16_t enAz, int16_t enCok) {
  if (deger < enAz) return enAz;
  if (deger > enCok) return enCok;
  return deger;
}

uint8_t bitSayisi(uint8_t deger) {
  return __builtin_popcount(static_cast<unsigned int>(deger));
}

bool desenlerAyni(const Desen &a, const Desen &b) {
  return memcmp(a.satir, b.satir, HUCRE_YUKSEKLIK) == 0;
}

uint8_t desenUzakligi(const Desen &a, const Desen &b) {
  uint8_t uzaklik = 0;
  for (uint8_t y = 0; y < HUCRE_YUKSEKLIK; y++) {
    uzaklik += bitSayisi((a.satir[y] ^ b.satir[y]) & B11111);
  }
  return uzaklik;
}

uint8_t desenGosterimHatasi(const Desen &hedef, const Desen &gosterilen) {
  uint8_t hata = 0;
  for (uint8_t y = 0; y < HUCRE_YUKSEKLIK; y++) {
    const uint8_t hedefSatir = hedef.satir[y] & B11111;
    const uint8_t gosterilenSatir = gosterilen.satir[y] & B11111;
    const uint8_t eksikPiksel = hedefSatir & ~gosterilenSatir;
    const uint8_t fazlaPiksel = gosterilenSatir & ~hedefSatir;

    // Arka planda yanlislikla yanan tek bir nokta, goz kenarindaki eksik bir
    // noktadan daha rahatsiz edicidir. Bu nedenle fazla piksel 5 kat cezali.
    hata += bitSayisi(eksikPiksel & B11111);
    hata += 5 * bitSayisi(fazlaPiksel & B11111);
  }
  return hata;
}

bool desenBos(const Desen &desen) {
  for (uint8_t y = 0; y < HUCRE_YUKSEKLIK; y++) {
    if ((desen.satir[y] & B11111) != 0) return false;
  }
  return true;
}

bool desenDolu(const Desen &desen) {
  for (uint8_t y = 0; y < HUCRE_YUKSEKLIK; y++) {
    if ((desen.satir[y] & B11111) != B11111) return false;
  }
  return true;
}

bool lcdBagliMi() {
  Wire.beginTransmission(LCD_ADRESI);
  return Wire.endTransmission() == 0;
}

// ---------------------------------------------------------------------------
// 100x32 PIKSEL CIZIM MOTORU
// ---------------------------------------------------------------------------

void tuvaliTemizle() {
  memset(pikselTamponu, 0, sizeof(pikselTamponu));
}

void pikselYaz(int16_t x, int16_t y, bool dolu) {
  if (x < 0 || x >= PIKSEL_GENISLIK || y < 0 || y >= PIKSEL_YUKSEKLIK) return;
  pikselTamponu[y][x] = dolu ? 1 : 0;
}

void gozBebegiOy(int16_t merkezX, int16_t merkezY, int8_t bakisX, int8_t bakisY) {
  const int16_t pupilX = merkezX + bakisX / 2;
  const int16_t pupilY = merkezY + bakisY / 2;

  // 7x11 elips biciminde bosluk.
  for (int8_t dy = -6; dy <= 6; dy++) {
    for (int8_t dx = -4; dx <= 4; dx++) {
      const int16_t elips = dx * dx * 36 + dy * dy * 16;
      if (elips <= 576) pikselYaz(pupilX + dx, pupilY + dy, false);
    }
  }

  // Kucuk yansima noktasi.
  pikselYaz(pupilX - 1, pupilY - 2, true);
}

void tekGozCiz(bool solGoz, int8_t bakisX, int8_t bakisY,
               int8_t ekstraX, int8_t ekstraY) {
  constexpr int8_t YARI_GENISLIK = 19;
  constexpr int8_t YARI_YUKSEKLIK = 10;
  constexpr int8_t KOSE_YARICAPI = 6;

  const int16_t temelMerkezX = solGoz ? 26 : 73;
  const int16_t merkezX = temelMerkezX + bakisX + ekstraX;
  const int16_t merkezY = 16 + bakisY + ekstraY;

  for (int8_t dx = -YARI_GENISLIK; dx <= YARI_GENISLIK; dx++) {
    // RoboEyes'e benzer yuvarlatilmis dikdortgen. Elips yerine bu geometriyi
    // kullanmak, gozleri daha robotik gosterirken 5x8 hucrelerdeki desen
    // cesitliligini azaltir ve sekiz CGRAM slotunun kalitesini yukselterek
    // kenarlardaki rastgele nokta gorunumunu azaltir.
    const int8_t mutlakX = abs(dx);
    int16_t koseGirintisi = 0;
    if (mutlakX > YARI_GENISLIK - KOSE_YARICAPI) {
      const int8_t koseX = mutlakX - (YARI_GENISLIK - KOSE_YARICAPI);
      const int16_t kare = KOSE_YARICAPI * KOSE_YARICAPI - koseX * koseX;
      koseGirintisi = KOSE_YARICAPI - static_cast<int16_t>(sqrtf(kare > 0 ? kare : 0));
    }

    int16_t ust = merkezY - YARI_YUKSEKLIK + koseGirintisi;
    int16_t alt = merkezY + YARI_YUKSEKLIK - koseGirintisi;

    // Ic kose: sol gozde sag, sag gozde sol taraftir.
    const int16_t icKoseOrani = solGoz
      ? (dx + YARI_GENISLIK)
      : (YARI_GENISLIK - dx);

    switch (aktifIfade) {
      case KIZGIN:
        // Ic koseye dogru inen ust kapak: otomobil fari etkisi.
        ust += (icKoseOrani * 6) / (YARI_GENISLIK * 2);
        break;

      case YORGUN:
        // Ust kapagi asagi indirir.
        ust += 5;
        break;

      case MUTLU:
        // Dis koseleri yukselten alt kapak, gulumseme etkisi verir.
        alt -= (abs(dx) * 5) / YARI_GENISLIK;
        break;

      case NORMAL:
      default:
        break;
    }

    if (alt < ust) alt = ust;

    // Kirpmada iki kapak merkeze yaklasir. Tam kapali halde tek piksel cizgi.
    const int16_t orta = (ust + alt) / 2;
    const int16_t ustUzaklik = orta - ust;
    const int16_t altUzaklik = alt - orta;
    ust = orta - (ustUzaklik * gozAcikligi) / 100;
    alt = orta + (altUzaklik * gozAcikligi) / 100;

    for (int16_t y = ust; y <= alt; y++) {
      pikselYaz(merkezX + dx, y, true);
    }
  }

  if (GOZ_BEBEGI_GOSTER && gozAcikligi >= 45) {
    gozBebegiOy(merkezX, merkezY, bakisX, bakisY);
  }
}

void gozleriTuvaleCiz(unsigned long simdi) {
  tuvaliTemizle();

  int8_t ekstraX = 0;
  int8_t ekstraY = 0;

  if (ozelAnimasyon != OZEL_YOK) {
    if ((long)(simdi - ozelAnimasyonBitisi) >= 0) {
      ozelAnimasyon = OZEL_YOK;
    } else {
      const uint8_t asama = (simdi - ozelAnimasyonBaslangici) / 65;
      const int8_t yon = (asama & 1) ? 1 : -1;
      if (ozelAnimasyon == OZEL_SASIRMA) ekstraX = 4 * yon;
      if (ozelAnimasyon == OZEL_GULME) ekstraY = 2 * yon;
    }
  }

  const int8_t bakisX = static_cast<int8_t>(bakisX100 / 100);
  const int8_t bakisY = static_cast<int8_t>(bakisY100 / 100);

  tekGozCiz(true, bakisX, bakisY, ekstraX, ekstraY);
  tekGozCiz(false, bakisX, bakisY, ekstraX, ekstraY);
}

// ---------------------------------------------------------------------------
// 100x32 TUVALI 80 ADET 5x8 HUCREYE BOLME
// ---------------------------------------------------------------------------

void hucreDesenleriniCikar() {
  for (uint8_t hucreY = 0; hucreY < LCD_SATIR; hucreY++) {
    for (uint8_t hucreX = 0; hucreX < LCD_SUTUN; hucreX++) {
      const uint8_t hucreNo = hucreY * LCD_SUTUN + hucreX;

      for (uint8_t yerelY = 0; yerelY < HUCRE_YUKSEKLIK; yerelY++) {
        uint8_t bitler = 0;
        for (uint8_t yerelX = 0; yerelX < HUCRE_GENISLIK; yerelX++) {
          const uint8_t x = hucreX * HUCRE_GENISLIK + yerelX;
          const uint8_t y = hucreY * HUCRE_YUKSEKLIK + yerelY;
          if (pikselTamponu[y][x]) bitler |= (B10000 >> yerelX);
        }
        hucreDesenleri[hucreNo].satir[yerelY] = bitler;
      }
    }
  }
}

void tekilDesenleriBul() {
  tekilDesenSayisi = 0;

  for (uint8_t h = 0; h < HUCRE_SAYISI; h++) {
    const Desen &aday = hucreDesenleri[h];

    // Bos ve tam dolu hucreler LCD'nin hazir karakterleriyle gosterilir;
    // sekiz ozel karakter hakkini tuketmez.
    if (desenBos(aday) || desenDolu(aday)) continue;

    int16_t bulunan = -1;
    for (uint8_t i = 0; i < tekilDesenSayisi; i++) {
      if (desenlerAyni(aday, tekilDesenler[i].desen)) {
        bulunan = i;
        break;
      }
    }

    if (bulunan >= 0) {
      tekilDesenler[bulunan].tekrar++;
    } else {
      tekilDesenler[tekilDesenSayisi].desen = aday;
      tekilDesenler[tekilDesenSayisi].tekrar = 1;
      tekilDesenSayisi++;
    }
  }
}

// ---------------------------------------------------------------------------
// DINAMIK 8-DESEN SIKISTIRMA
// ---------------------------------------------------------------------------

uint8_t enYakinCgram(const Desen &desen, const Desen prototipler[]) {
  uint8_t enIyi = 0;
  uint8_t enIyiUzaklik = 255;

  for (uint8_t k = 0; k < OZEL_DESEN_SAYISI; k++) {
    const uint8_t uzaklik = desenGosterimHatasi(desen, prototipler[k]);
    if (uzaklik < enIyiUzaklik) {
      enIyiUzaklik = uzaklik;
      enIyi = k;
    }
  }
  return enIyi;
}

void sekizdenAzDeseniYerlestir(Desen yeniPrototipler[]) {
  bool kullanilanSlot[OZEL_DESEN_SAYISI] = {false};
  bool kullanilanDesen[OZEL_DESEN_SAYISI] = {false};

  // Sik gorunen desenleri once yerlestirir. Eski CGRAM'a en yakin bos slotu
  // secmek, kareler arasinda karakterlerin yer degistirmesini azaltir.
  for (uint8_t sira = 0; sira < tekilDesenSayisi; sira++) {
    int16_t secilenDesen = -1;
    uint8_t enCokTekrar = 0;
    for (uint8_t i = 0; i < tekilDesenSayisi; i++) {
      if (!kullanilanDesen[i] && tekilDesenler[i].tekrar > enCokTekrar) {
        enCokTekrar = tekilDesenler[i].tekrar;
        secilenDesen = i;
      }
    }

    if (secilenDesen < 0) break;
    kullanilanDesen[secilenDesen] = true;

    uint8_t secilenSlot = 0;
    uint8_t enIyiUzaklik = 255;
    for (uint8_t k = 0; k < OZEL_DESEN_SAYISI; k++) {
      if (kullanilanSlot[k]) continue;
      const uint8_t uzaklik = cgramHazir
        ? desenUzakligi(tekilDesenler[secilenDesen].desen, aktifCgram[k])
        : k;
      if (uzaklik < enIyiUzaklik) {
        enIyiUzaklik = uzaklik;
        secilenSlot = k;
      }
    }

    yeniPrototipler[secilenSlot] = tekilDesenler[secilenDesen].desen;
    kullanilanSlot[secilenSlot] = true;
  }
}

void ilkPrototipleriSec(Desen prototipler[]) {
  // Ilk merkez: ekranda en cok gorunen kismi temsil eden desen.
  uint8_t ilk = 0;
  for (uint8_t i = 1; i < tekilDesenSayisi; i++) {
    if (tekilDesenler[i].tekrar > tekilDesenler[ilk].tekrar) ilk = i;
  }
  prototipler[0] = tekilDesenler[ilk].desen;

  // Sonraki merkezler: secilmis merkezlerden en uzak desenler.
  for (uint8_t k = 1; k < OZEL_DESEN_SAYISI; k++) {
    uint32_t enIyiPuan = 0;
    uint8_t secilen = 0;

    for (uint8_t i = 0; i < tekilDesenSayisi; i++) {
      uint8_t enYakinUzaklik = 255;
      for (uint8_t onceki = 0; onceki < k; onceki++) {
        const uint8_t uzaklik = desenUzakligi(tekilDesenler[i].desen, prototipler[onceki]);
        if (uzaklik < enYakinUzaklik) enYakinUzaklik = uzaklik;
      }

      // Seyrek gorunen kose/kapak desenleri de onemlidir; frekansla carpmak
      // bu ince kenarlari tamamen yok edebildigi icin cesitlilik puani yalnizca
      // en yakin merkeze olan uzakliga dayanir.
      const uint32_t puan = enYakinUzaklik;
      if (puan > enIyiPuan) {
        enIyiPuan = puan;
        secilen = i;
      }
    }
    prototipler[k] = tekilDesenler[secilen].desen;
  }
}

void prototipleriKumele(Desen prototipler[]) {
  uint8_t atama[HUCRE_SAYISI];

  // Dort tur, en fazla 80 desen icin ESP32-S3'te cok hafiftir.
  for (uint8_t tur = 0; tur < 4; tur++) {
    uint8_t kumeSayisi[OZEL_DESEN_SAYISI] = {0};

    for (uint8_t i = 0; i < tekilDesenSayisi; i++) {
      atama[i] = enYakinCgram(tekilDesenler[i].desen, prototipler);
      kumeSayisi[atama[i]]++;
    }

    for (uint8_t k = 0; k < OZEL_DESEN_SAYISI; k++) {
      if (kumeSayisi[k] == 0) {
        // Bos merkez olursa mevcut merkezlerden en uzak deseni buraya al.
        uint8_t secilen = 0;
        uint8_t enUzak = 0;
        for (uint8_t i = 0; i < tekilDesenSayisi; i++) {
          const uint8_t uzaklik = desenUzakligi(
            tekilDesenler[i].desen,
            prototipler[enYakinCgram(tekilDesenler[i].desen, prototipler)]
          );
          if (uzaklik > enUzak) {
            enUzak = uzaklik;
            secilen = i;
          }
        }
        prototipler[k] = tekilDesenler[secilen].desen;
        continue;
      }

      // Kume merkezi olarak gercek desenlerden toplam hatasi en az olani sec.
      uint32_t enIyiMaliyet = 0xFFFFFFFFUL;
      int16_t enIyiAday = -1;

      for (uint8_t aday = 0; aday < tekilDesenSayisi; aday++) {
        if (atama[aday] != k) continue;

        uint32_t maliyet = 0;
        for (uint8_t diger = 0; diger < tekilDesenSayisi; diger++) {
          if (atama[diger] != k) continue;
          maliyet += desenGosterimHatasi(tekilDesenler[diger].desen,
                                        tekilDesenler[aday].desen);
        }

        if (maliyet < enIyiMaliyet) {
          enIyiMaliyet = maliyet;
          enIyiAday = aday;
        }
      }

      if (enIyiAday >= 0) prototipler[k] = tekilDesenler[enIyiAday].desen;
    }
  }
}

void enIyiSekizDeseniOlustur(Desen yeniPrototipler[]) {
  // Kullanilmayan slotlar kareler arasinda ayni kalsin.
  for (uint8_t k = 0; k < OZEL_DESEN_SAYISI; k++) {
    yeniPrototipler[k] = aktifCgram[k];
  }

  if (tekilDesenSayisi == 0) return;

  if (tekilDesenSayisi <= OZEL_DESEN_SAYISI) {
    sekizdenAzDeseniYerlestir(yeniPrototipler);
    return;
  }

  if (!cgramHazir) {
    ilkPrototipleriSec(yeniPrototipler);
  }

  // Onceki karenin prototipleri merkez olarak kullanilir. Bu, slot kimligini
  // korur ve CGRAM degisiminden kaynaklanan titremeyi azaltir.
  prototipleriKumele(yeniPrototipler);
}

void cgramiGuncelle(const Desen yeniPrototipler[]) {
  for (uint8_t k = 0; k < OZEL_DESEN_SAYISI; k++) {
    if (!cgramHazir || !desenlerAyni(yeniPrototipler[k], aktifCgram[k])) {
      uint8_t satirlar[HUCRE_YUKSEKLIK];
      for (uint8_t y = 0; y < HUCRE_YUKSEKLIK; y++) {
        satirlar[y] = yeniPrototipler[k].satir[y] & B11111;
      }
      lcd.createChar(k, satirlar);
      aktifCgram[k] = yeniPrototipler[k];
    }
  }
  cgramHazir = true;
}

void hucreHaritasiniOlustur(const Desen prototipler[]) {
  for (uint8_t h = 0; h < HUCRE_SAYISI; h++) {
    if (desenBos(hucreDesenleri[h])) {
      yeniHucreHaritasi[h] = BOS_KARAKTER;
    } else if (desenDolu(hucreDesenleri[h])) {
      yeniHucreHaritasi[h] = TAM_BLOK_KARAKTER;
    } else {
      yeniHucreHaritasi[h] = enYakinCgram(hucreDesenleri[h], prototipler);
    }
  }
}

void hucreHaritasiniEkranaYaz() {
  for (uint8_t satir = 0; satir < LCD_SATIR; satir++) {
    int8_t ilkDegisen = -1;
    int8_t sonDegisen = -1;

    for (uint8_t sutun = 0; sutun < LCD_SUTUN; sutun++) {
      const uint8_t h = satir * LCD_SUTUN + sutun;
      if (yeniHucreHaritasi[h] != eskiHucreHaritasi[h]) {
        if (ilkDegisen < 0) ilkDegisen = sutun;
        sonDegisen = sutun;
      }
    }

    if (ilkDegisen < 0) continue;

    // Ilk ve son degisen hucre arasini tek akista yazmak, her hucrede yeniden
    // setCursor() cagirmaktan daha hizlidir.
    lcd.setCursor(ilkDegisen, satir);
    for (int8_t sutun = ilkDegisen; sutun <= sonDegisen; sutun++) {
      const uint8_t h = satir * LCD_SUTUN + sutun;
      lcd.write(static_cast<uint8_t>(yeniHucreHaritasi[h]));
      eskiHucreHaritasi[h] = yeniHucreHaritasi[h];
    }
  }
}

void kareyiEkranaGonder() {
  hucreDesenleriniCikar();
  tekilDesenleriBul();

  Desen yeniPrototipler[OZEL_DESEN_SAYISI];
  enIyiSekizDeseniOlustur(yeniPrototipler);
  cgramiGuncelle(yeniPrototipler);
  hucreHaritasiniOlustur(aktifCgram);
  hucreHaritasiniEkranaYaz();
}

// ---------------------------------------------------------------------------
// ROBOEYES BENZERI HAREKET MANTIGI
// ---------------------------------------------------------------------------

void kirpmayiBaslat(unsigned long simdi) {
  if (kirpmaDurumu != KIRPMA_BEKLEME) return;
  kalanKirpma = (CIFT_KIRPMA && random(100) < 18) ? 2 : 1;
  kirpmaDurumu = KIRPMA_KAPANIYOR;
  kirpmaAsamasiBaslangici = simdi;
}

void kirpmayiGuncelle(unsigned long simdi) {
  const unsigned long gecen = simdi - kirpmaAsamasiBaslangici;

  switch (kirpmaDurumu) {
    case KIRPMA_BEKLEME:
      gozAcikligi = 100;
      if (OTOMATIK_KIRPMA && (long)(simdi - sonrakiKirpma) >= 0) {
        kirpmayiBaslat(simdi);
      }
      break;

    case KIRPMA_KAPANIYOR:
      if (gecen >= 125) {
        gozAcikligi = 0;
        kirpmaDurumu = KIRPMA_KAPALI;
        kirpmaAsamasiBaslangici = simdi;
      } else {
        const uint8_t ilerleme = (gecen * 100) / 125;
        gozAcikligi = 100 - (ilerleme * ilerleme) / 100;
      }
      break;

    case KIRPMA_KAPALI:
      gozAcikligi = 0;
      if (gecen >= 65) {
        kirpmaDurumu = KIRPMA_ACILIYOR;
        kirpmaAsamasiBaslangici = simdi;
      }
      break;

    case KIRPMA_ACILIYOR:
      if (gecen >= 175) {
        gozAcikligi = 100;
        kalanKirpma--;
        kirpmaAsamasiBaslangici = simdi;

        if (kalanKirpma > 0) {
          kirpmaDurumu = KIRPMA_ARASI;
        } else {
          kirpmaDurumu = KIRPMA_BEKLEME;
          sonrakiKirpma = simdi + random(2300, 5601);
        }
      } else {
        const uint8_t ilerleme = (gecen * 100) / 175;
        // Acilis sonunda yavaslayan ease-out egri.
        const uint16_t ters = 100 - ilerleme;
        gozAcikligi = 100 - (ters * ters) / 100;
      }
      break;

    case KIRPMA_ARASI:
      gozAcikligi = 100;
      if (gecen >= 130) {
        kirpmaDurumu = KIRPMA_KAPANIYOR;
        kirpmaAsamasiBaslangici = simdi;
      }
      break;
  }
}

void bakisiGuncelle(unsigned long simdi) {
  if (OTOMATIK_BAKIS && (long)(simdi - sonrakiBakis) >= 0) {
    int8_t yeniX;
    int8_t yeniY;

    do {
      yeniX = random(-5, 6);
      yeniY = random(-3, 4);
    } while (yeniX == hedefBakisX && yeniY == hedefBakisY);

    hedefBakisX = yeniX;
    hedefBakisY = yeniY;
    sonrakiBakis = simdi + random(900, 2601);
  }

  // Dusuk geciren filtre: hedefe bir anda atlamak yerine yumusakca yaklasir.
  const int16_t hedefX100 = hedefBakisX * 100;
  const int16_t hedefY100 = hedefBakisY * 100;
  bakisX100 += (hedefX100 - bakisX100) / 3;
  bakisY100 += (hedefY100 - bakisY100) / 3;

  if (abs(hedefX100 - bakisX100) < 3) bakisX100 = hedefX100;
  if (abs(hedefY100 - bakisY100) < 3) bakisY100 = hedefY100;
}

void ifadeyiGuncelle(unsigned long simdi) {
  if (!OTOMATIK_IFADE || (long)(simdi - sonrakiIfade) < 0) return;

  Ifade yeniIfade;
  do {
    yeniIfade = static_cast<Ifade>(random(0, 4));
  } while (yeniIfade == aktifIfade);

  aktifIfade = yeniIfade;
  sonrakiIfade = simdi + random(8000, 15001);
}

void ozelAnimasyonuBaslat(OzelAnimasyon animasyon, unsigned long simdi) {
  ozelAnimasyon = animasyon;
  ozelAnimasyonBaslangici = simdi;
  ozelAnimasyonBitisi = simdi + 900;
}

void seriKomutlariOku(unsigned long simdi) {
  while (Serial.available() > 0) {
    const char komut = Serial.read();
    switch (komut) {
      case '1': aktifIfade = NORMAL;  break;
      case '2': aktifIfade = KIZGIN;  break;
      case '3': aktifIfade = YORGUN;  break;
      case '4': aktifIfade = MUTLU;   break;
      case 'b':
      case 'B': kirpmayiBaslat(simdi); break;
      case 's':
      case 'S': ozelAnimasyonuBaslat(OZEL_SASIRMA, simdi); break;
      case 'g':
      case 'G': ozelAnimasyonuBaslat(OZEL_GULME, simdi); break;
      default: break;
    }
  }
}

void animasyonuGuncelle(unsigned long simdi) {
  kirpmayiGuncelle(simdi);
  bakisiGuncelle(simdi);
  ifadeyiGuncelle(simdi);
}

// ---------------------------------------------------------------------------
// ARDUINO
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  Wire.setClock(I2C_HIZI);

  if (!lcdBagliMi()) {
    Serial.println("LCD bulunamadi. Baglantilari ve 0x27/0x3F adresini kontrol edin.");
  }

  lcd.init();
  lcd.backlight();

  memset(aktifCgram, 0, sizeof(aktifCgram));
  memset(eskiHucreHaritasi, 254, sizeof(eskiHucreHaritasi));

  randomSeed(esp_random());

  const unsigned long simdi = millis();
  sonrakiKirpma = simdi + 1700;
  sonrakiBakis = simdi + 800;
  sonrakiIfade = simdi + 10000;
  sonrakiKare = simdi;

  Serial.println("RoboEyes LCD 20x4 basladi.");
  Serial.println("Komutlar: 1=normal 2=kizgin 3=yorgun 4=mutlu B=kirp S=sasir G=gul");
}

void loop() {
  const unsigned long simdi = millis();
  seriKomutlariOku(simdi);

  if ((long)(simdi - sonrakiKare) >= 0) {
    // Program gecikmisse yuzlerce eski kareyi yakalamaya calisma.
    sonrakiKare = simdi + KARE_SURESI_MS;

    animasyonuGuncelle(simdi);
    gozleriTuvaleCiz(simdi);
    kareyiEkranaGonder();
  }

  // Motor, sensor ve kumanda kodlarinizi buraya ekleyebilirsiniz.
  // delay() eklemeyin; animasyon ve diger islemler birlikte ilerlesin.
}