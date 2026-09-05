# Unit tests

Host-based unit tests for `sketch_jul22a.ino`.

The sketch targets an ESP32 (Deneyap Kart 1A v2), but its drawing, image
compression and animation logic is pure C++. These tests compile that logic
with a standard host toolchain (`g++`) using small Arduino mocks in
[`mock/`](./mock), so the core engine can be verified without hardware.

## Running

```bash
cd sketch_jul22a/test
./run_tests.sh        # or: make
```

Line coverage of the sketch:

```bash
make coverage         # requires gcov (ships with gcc)
```

## What is covered

| Module | Functions under test |
|---|---|
| numeric helpers | `sinirla16`, `ilerleme1000`, `yumusakIlerleme1000`, `hizliBasla1000`, `bitSayisi` |
| pattern ops | `desenlerAyni`, `desenBos`, `desenDolu`, `desenUzakligi`, `desenGosterimHatasi` |
| canvas engine | `tuvaliTemizle`, `pikselYaz`, `doluDikdortgenCiz`, `doluElipsCiz`, `cizgiCiz` |
| cell extraction | `hucreDesenleriniCikar`, `tekilDesenleriBul` |
| 8-pattern compression | `enYakinCgram`, `enIyiSekizDeseniOlustur`, `aktifCgramTekTipMi`, `hucreHaritasiniOlustur` |
| display pipeline | `hucreHaritasiniEkranaYaz`, `kareyiEkranaGonder` |
| blink state machine | `kirpmayiBaslat`, `kirpmayiGuncelle` |
| gaze | `bakisiGuncelle` |
| special animations | `ozelAnimasyonuBaslat` |
| serial commands | `seriKomutlariOku` |
| hardware presence | `lcdBagliMi` |
| punch scene | `yumrukSahnesiniBaslat`, `yumrukSahnesiniTuvaleCiz` |

## How it works

`test_sketch.cpp` `#include`s the sketch directly. The build force-includes
[`mock/Arduino.h`](./mock/Arduino.h) and puts `mock/` first on the include path,
so the sketch's `<Wire.h>`, `<LiquidCrystal_I2C.h>` and `<esp_system.h>`
includes resolve to the mocks. The mocks make timing (`millis`), randomness
(`random`) and I/O (`Serial`, `Wire`, the LCD) deterministic and inspectable.
