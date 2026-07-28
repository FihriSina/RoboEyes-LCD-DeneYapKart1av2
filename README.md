# RoboEyes LCD 20x4 – Deneyap Kart 1A v2

20×4 karakter LCD üzerinde otomobil farlarını andıran hareketli robot gözleri oluşturmak için geliştirilmiş Arduino projesidir. Kod; göz kırpma, çift kırpma, yumuşak bakış hareketleri ve farklı yüz ifadeleri sunar.

HD44780 tabanlı karakter LCD kullanabilmek için çizim ve görüntü sıkıştırma motoru bu proje kapsamında yeniden geliştirilmiştir.

## Özellikler

- 20×4 LCD'nin dört satırını ve 20 sütununu kullanır.
- Görüntüyü RAM üzerinde `100×32` sanal piksel olarak çizer.
- Her animasyon karesini LCD'nin sekiz özel karakter sınırına otomatik olarak sıkıştırır.
- Otomatik göz kırpma ve ara sıra çift kırpma yapar.
- Gözleri yumuşak biçimde sağa, sola, yukarıya ve aşağıya hareket ettirir.
- Normal, kızgın, yorgun ve mutlu ifadeleri destekler.
- Buckshot Roulette'in karanlık masa/krupiye atmosferinden esinlenen; ancak LCD için özgün çizilmiş krupiye, şüpheci, hasarlı ve çılgın tiplemeleri içerir.
- Darbe alma ve gerilim bakışı animasyonları sunar.
- Seri Monitör'den tetiklenen, sağdan sola ilerleyen belirgin bir yumruk animasyonu içerir.
- Şaşırma ve gülme benzeri kısa animasyonlar içerir.
- `delay()` kullanmaz; sensör, motor ve kumanda kodlarıyla birlikte çalışabilir.
- Yalnızca değişen LCD hücrelerini güncelleyerek ekran titremesini azaltır.
- I²C hatalarını Seri Monitör'e bildirir; bağlantı koparsa ekranı saniyede bir yeniden kurmayı dener.

## Çalışma mantığı

20×4 LCD'de 80 karakter hücresi bulunur. Her hücre 5×8 noktadan oluştuğu için ekranda fiziksel olarak:

```text
20 × 4 × 5 × 8 = 3200 nokta
```

vardır. Bu, `100×32` boyutunda bir çizim alanına karşılık gelir. Ancak HD44780 denetleyicisi bu noktaların tamamını bağımsız bir grafik ekran gibi kontrol ettirmez. Aynı anda yalnızca sekiz farklı 5×8 özel karakter tanımlanabilir.

Kod bu sınıra şu yöntemle yaklaşır:

1. Gözleri RAM üzerinde `100×32` sanal piksel tuvaline çizer.
2. Tuvali 80 adet 5×8 hücre desenine böler.
3. Boş ve tam dolu hücreleri LCD'nin hazır karakterleriyle gösterir.
4. Kalan desenleri en uygun sekiz özel desene sıkıştırır.
5. Özel karakter belleğini ve değişen ekran hücrelerini dinamik olarak günceller.

Bu yöntem LCD'nin tüm fiziksel nokta alanından yararlanır; fakat aynı anda yalnızca sekiz benzersiz özel desen kullanılabildiği için OLED kadar pürüzsüz sonuç vermez.

## Yumruk animasyonu

Kart açıldığında önceki proje sürümündeki gibi doğrudan göz animasyonu başlar. Yumruk sahnesi otomatik oynatılmaz.

Seri Monitör'e `Y` gönderildiğinde:

1. Sağ yumruk ekranın sağından girer.
2. Belirgin ve okunaklı bir hareketle sağdan sola tüm ekranı geçer.
3. Darbe anında yıldız, hareket çizgileri ve kısa sarsıntı gösterilir.
4. Yaklaşık 1,5 saniye sonra göz animasyonu kaldığı yerden devam eder.

Bu sahne çalışırken `delay()` kullanılmaz. Eski sürümle uyumluluk için `P` komutu da aynı animasyonu başlatır.

## Gerekli donanımlar

- Deneyap Kart 1A v2
- 20×4 HD44780 uyumlu karakter LCD
- PCF8574 tabanlı I²C LCD dönüştürücü
- Dört adet bağlantı kablosu
- Gerektiğinde çift yönlü I²C seviye dönüştürücü

## Bağlantılar

Kablo renklerine değil, modül üzerindeki pin adlarına göre bağlantı yapın.

| LCD I²C pini | Deneyap Kart 1A v2 |
|---|---|
| `GND` | `GND` |
| `VCC` | `3V3` |
| `SDA` | `SDA` |
| `SCL` | `SCL` |

> [!CAUTION]
> İlk denemeyi LCD'yi `3V3` ile besleyerek yapın. LCD yalnızca 5 V ile çalışıyorsa SDA ve SCL hatlarında çift yönlü I²C seviye dönüştürücü kullanın. Deneyap Kart 1A v2 pinlerine doğrudan 5 V I²C sinyali uygulamayın.

## Yazılım kurulumu

1. Arduino IDE'yi açın.
2. Deneyap Kart kart paketini kurun.
3. Kart olarak **Deneyap Kart 1A v2** seçin.
4. Kütüphane Yöneticisi'nden **LiquidCrystal I2C** kütüphanesini kurun.
5. [`RoboEyes_LCD20x4_Deneyap_1A_v2.ino`](./RoboEyes_LCD20x4_Deneyap_1A_v2.ino) dosyasını açın.
6. Kartı bilgisayara bağlayıp kodu yükleyin.
7. Seri Monitör'ü `115200 baud` hızında açın.

## Seri Monitör komutları

Seri Monitör'e aşağıdaki karakterlerden birini göndererek animasyonu kontrol edebilirsiniz:

| Komut | İşlev |
|---|---|
| `1` | Normal ifade |
| `2` | Kızgın otomobil gözü |
| `3` | Yorgun ifade |
| `4` | Mutlu ifade |
| `5` | Krupiye — karanlık ve tehditkâr bakış |
| `6` | Şüpheci — tek gözü kısık bakış |
| `7` | Hasarlı — kısık ve çizikli göz |
| `8` | Çılgın — farklı yükseklikte asimetrik gözler |
| `B` veya `b` | Göz kırpma |
| `S` veya `s` | Şaşırma/sallanma animasyonu |
| `G` veya `g` | Gülme animasyonu |
| `H` veya `h` | Darbe alma — sert sarsıntı ve göz kısma |
| `T` veya `t` | Gerilim — gözleri yavaşça kısıp açma |
| `Y` veya `y` | Sağ yumruğu sağdan sola oynatır |
| `P` veya `p` | Yumruk için eski komut; `Y` ile aynı işlev |
| `?` | Komut menüsünü yeniden yazdırır |

Varsayılan başlangıç ifadesi kızgın otomobil gözüdür.

## Kullanıcı ayarları

Temel seçenekler `.ino` dosyasının başındaki **KULLANICI AYARLARI** bölümündedir.

### LCD adresi

Varsayılan I²C adresi:

```cpp
constexpr uint8_t LCD_ADRESI = 0x27;
```

Ekran bulunamazsa `0x3F` değerini deneyin:

```cpp
constexpr uint8_t LCD_ADRESI = 0x3F;
```

### I²C hızı

Güvenli başlangıç değeri:

```cpp
constexpr uint32_t I2C_HIZI = 100000;
```

LCD kararlı çalışıyorsa daha hızlı güncelleme için:

```cpp
constexpr uint32_t I2C_HIZI = 400000;
```

değerini deneyebilirsiniz. Görüntü bozulursa tekrar `100000` değerine dönün.

### Kare hızı

Varsayılan değer:

```cpp
constexpr uint8_t MAKS_KARE_HIZI = 12;
```

400 kHz I²C bağlantısı kararlıysa `15` FPS denenebilir:

```cpp
constexpr uint8_t MAKS_KARE_HIZI = 15;
```

### Başlangıç ifadesi

```cpp
constexpr Ifade BASLANGIC_IFADESI = KIZGIN;
```

Kullanılabilir ifadeler:

```cpp
NORMAL
KIZGIN
YORGUN
MUTLU
KRUPIYE
SUPHECI
HASARLI
CILGIN
```

### Otomatik davranışlar

```cpp
constexpr bool OTOMATIK_IFADE = false;
constexpr bool OTOMATIK_BAKIS = true;
constexpr bool OTOMATIK_KIRPMA = true;
constexpr bool CIFT_KIRPMA = true;
```

`OTOMATIK_IFADE` değeri `true` yapılırsa ifade 8–15 saniyede bir kendiliğinden değişir.

### Açılış davranışı

İlk proje sürümündeki gibi kart açıldığında doğrudan gözleri göstermek için:

```cpp
constexpr bool ACILISTA_YUMRUK_SAHNESI = false;
```

Yumruğun her açılışta otomatik oynatılmasını isterseniz değeri `true` yapın:

```cpp
constexpr bool ACILISTA_YUMRUK_SAHNESI = true;
```

### Göz bebeği

Varsayılan otomobil farı görünümünde gözler doludur:

```cpp
constexpr bool GOZ_BEBEGI_GOSTER = false;
```

Göz bebeği eklemek için:

```cpp
constexpr bool GOZ_BEBEGI_GOSTER = true;
```

değerini kullanın. Sekiz özel desen sınırı nedeniyle göz bebeği açıkken kenar ayrıntıları bir miktar azalabilir.

## Başka kodlarla birlikte kullanma

Animasyon zamanlaması `millis()` ile yapılır. Motor, sensör veya uzaktan kumanda kodlarınızı `loop()` fonksiyonunun sonuna ekleyebilirsiniz:

```cpp
void loop() {
  const unsigned long simdi = millis();
  seriKomutlariOku(simdi);

  if (!lcdCalisiyor && (long)(simdi - sonrakiLcdKurtarma) >= 0) {
    lcdBaslat(simdi);
  }

  if ((long)(simdi - sonrakiKare) >= 0) {
    sonrakiKare = simdi + KARE_SURESI_MS;

    if (aktifSahne == SAHNE_YUMRUK_GIRISI) {
      yumrukSahnesiniTuvaleCiz(simdi);
    } else {
      animasyonuGuncelle(simdi);
      gozleriTuvaleCiz(simdi);
    }
    kareyiEkranaGonder(simdi);
  }

  // Sensor, motor ve kumanda kodlari buraya eklenebilir.
}
```

Animasyonun akıcı kalması için `loop()` içinde uzun `delay()` çağrıları kullanmayın.

## Sorun giderme

### Ekran hiç açılmıyor

- `VCC` ve `GND` bağlantılarını kontrol edin.
- SDA ve SCL kablolarının doğru pinlere bağlı olduğundan emin olun.
- USB kablosunun veri aktarımını desteklediğini doğrulayın.

### Arka ışık yanıyor fakat görüntü yok

- I²C adresini `0x27` yerine `0x3F` yaparak deneyin.
- LCD dönüştürücüsündeki kontrast potansiyometresini yavaşça çevirin.
- Seri Monitör'deki `LCD bulunamadi` ve `LCD hatasi (...)` mesajlarını kontrol edin.

### Ekran çalışırken kararıyor veya donuyor

Kod her karenin sonunda LCD'yi I²C üzerinden yoklar. Yanıt gelmezse Seri Monitör'e `LCD hatasi (kare yazma): ...` yazar, özel karakter ve hücre önbelleklerini geçersiz kılar ve saniyede bir yeniden bağlanmayı dener. Bağlantı geri geldiğinde `LCD yeniden baglandi.` mesajı görünür ve ekranın tamamı yeniden çizilir.

### Görüntü bozuk veya titriyor

- `I2C_HIZI` değerini `100000` yapın.
- `MAKS_KARE_HIZI` değerini `10` veya `12` yapın.
- Bağlantı kablolarını mümkün olduğunca kısa tutun.
- LCD'yi 5 V ile besliyorsanız uygun I²C seviye dönüştürücü kullandığınızdan emin olun.

### Kod derlenmiyor

- `LiquidCrystal I2C` kütüphanesinin kurulu olduğunu kontrol edin.
- Doğru kartı ve doğru USB portunu seçin.
- Deneyap Kart kart paketini güncelleyin.

## Proje dosyası

- [`RoboEyes_LCD20x4_Deneyap_1A_v2.ino`](./RoboEyes_LCD20x4_Deneyap_1A_v2.ino): Sanal piksel çizim, dinamik desen sıkıştırma ve animasyon kodu.

## Teknik sınırlamalar

- Ekran fiziksel olarak 3200 noktaya sahip olsa da yalnızca sekiz farklı özel 5×8 desen aynı anda tanımlanabilir.
- Hücreler arasında fiziksel boşluk bulunduğundan şekillerde hafif ızgara görünümü oluşur.
- CGRAM güncellemeleri I²C üzerinden yapıldığı için animasyon hızı grafik OLED'e göre düşüktür.
- Bu nedenle proje, karakter LCD'den alınabilecek yüksek görsel etkiyi hedefler; SSD1306 veya SH1106 OLED'deki RoboEyes görüntüsünün birebir aynısını üretmez.

## Kaynak ve teşekkür

Animasyon davranışı ve robot göz tasarımı için ilham kaynağı:

Bu depodaki Arduino kodu, 20×4 karakter LCD'nin HD44780/CGRAM yapısı için bağımsız olarak geliştirilmiştir.
