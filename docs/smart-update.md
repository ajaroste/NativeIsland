# NativeIsland 0.6.0

## Akıllı bildirimler

Bildirimler yaklaşık 2,6 saniye görünür, odağı çalmaz ve kapalı paneli zorla açmaz. Küçük adada mevcut bilgi yerini bildirime bırakır; açık panelde alt şerit kullanılır. Elle gizlenen veya tam ekran nedeniyle gizlenen adayı bildirimler yeniden göstermez. Ayarlardaki Akıllı bildirimler tümünü kapatır.

| Olay | Görsel davranış |
| --- | --- |
| Ses seviyesi | Yüzde ve seviye çubuğu; ardışık değişiklikler aynı bildirimi günceller, süre son değişiklikten itibaren uzar |
| Mikrofon sessize alma | Kırmızı, çizgili mikrofon simgesi; 300 ms boyunca azalan, en fazla 3 px yatay titreşim |
| Mikrofon açma | Mikrofon simgesi ve hafif büyüyüp geri oturan vurgu; titreşim yok |
| Ses aygıtı / Bluetooth bağlantısı | Aygıt adı, kulaklık simgesi ve yaklaşık 400 ms küçük vurgu |
| Düşük pil | Kehribar pil simgesi; şarj olmuyorken %20 ve %10 eşiklerinde birer uyarı. Şarj başlayınca veya %25 üstüne çıkınca yeniden hazırlanır |

Başlangıçtaki ses durumu bildirim sayılmaz. Pil yok/ölçüm bilinmiyor durumunda uyarı üretilmez. Hareketi azalt seçeneği titreşim, büyüme ve solmayı kapatır; statik bilgi korunur. Bildirimlerin listesi tutulmaz; yeni olay mevcut bildirimi değiştirebilir. Mikrofon gibi önemli bir uyarı ilk 700 ms'de seviye değişimlerine göre önceliklidir.

## Ses kontrolü

Medya sekmesinin altında gerçek ses çıkışı, tıklanabilir/sürüklenebilir seviye çubuğu, çıkış sessiz düğmesi, mikrofon aç/kapat ve Çıkış seç menüsü bulunur. İşlemler bir işçi iş parçacığında uygulanır; arayüz gerçek endpoint durumunu geri okuyarak güncellenir. Ses düzeyi/mute değişimleri yaklaşık 100 ms aralıkla, çıkış listesi yaklaşık 2 saniye aralıkla kontrol edilir. Bluetooth container eşlemesi mevcut sistem izleyicisinin tarama aralığını kullanır.

Seviye ve mikrofon kontrolü belgelenmiş `IAudioEndpointVolume` kullanır. Kontroller varsayılan **eConsole** çıkışı ve mikrofonu içindir; farklı mikrofonu seçmiş uygulamalar veya ayrı iletişim rolü bu düğmeyle mutlaka sessizleşmez. Mikrofon dinlenmez, ses kaydı alınmaz. Bu bir donanımsal gizlilik anahtarı değildir.

Çıkış seçimi Windows'un özel `IPolicyConfig` arayüzünü kullanır; eConsole ve eMultimedia rollerini değiştirir, eCommunications rolünü değiştirmez. İkinci rol başarısızsa eski roller geri yüklenmeye çalışılır; bir hata sessizce başarı sayılmaz. Özel arayüz bulunamazsa Windows ses ayarları açılır. Arayüzün bulunması, bütün sürücü/Windows sürümlerinde geçişin başarılı olacağını garanti etmez.

Referanslar: [Microsoft endpoint volume](https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nn-endpointvolume-iaudioendpointvolume), [özel arayüzün mevcut ABI tanımı](https://github.com/frgnca/AudioDeviceCmdlets/blob/master/SOURCE/IPolicyConfig.cs).

## Pano

- Arama satırına tıklayın. Ayrı küçük penceredeki sorgu yazdıkça süzülür; metin araması büyük/küçük harfe duyarsızdır. Görseller `görsel` / `image` sorgularıyla bulunur.
- Yıldız kayıtları sabitler; sabitler sonuçların başına çıkar. Yıldız tekrar basıldığında sabitleme kalkar.
- Temizle yalnızca sabit olmayan kayıtları kaldırır; tek kaydın x düğmesi sabit olsa da o kaydı kaldırır.
- Kapasite 100 kayıt / 64 MiB. Limitte eski sabit olmayan kayıtlar önce çıkarılır; bütün kayıtlar sabitse yeni girdiler alınmayabilir. Sabitleme sınırsız RAM kullanımına izin vermez.
- Otomatik temizleme: kapalı, 5/15/30 dakika, 1/2/24 saat. Varsayılan 30 dakika. Sabit kayıtlar varsayılan olarak süre temizliğinden muaf; ayrı seçenek onları da kapsar. Süre ilk yakalanma zamanından hesaplanır.
- Geçmiş RAM'dedir ve kapanınca kaybolur; sabitler diske kaydedilmez. İzlemeyi kapatmak tüm geçmişi, sabitler dahil temizler.
- Temizlenen şey **NativeIsland geçmişidir**; mevcut Windows pano içeriği değiştirilmez. Kullanıcının zaten açtığı önizleme pencereleri ayrı kopyalar tutar ve otomatik olarak kapatılmaz. Bu mekanizma güvenli RAM silme garantisi değildir.

## Doğrulama ve kullanım

12.10.2026 sonuçları: Release derlemesi başarılı, CTest 1/1 geçti, yerleşik görüntü/geometri testi çıkış kodu 0 ve 30 saniyelik çalışma testi çıkış kodu 0. Gerçek sistemde üç ses çıkışı okundu ve çıkış değiştirme COM arayüzünün varlığı doğrulandı; gerçek mute/çıkış değişikliği yapılmadı. Kurulum paketi yeni 0.6.0 EXE'siyle üretildi.

Core testleri arama, sabit önceliği, süre temizliği, sabit temizleme tercihi ve azaltılmış hareketi kapsar. Yerleşik PNG doğrulaması gerçek ses aygıtlarını salt okunur inceler; arama/sabit yıldızları ve mikrofon bildirimi için açıkça izole test girdileri çizer. Testler Windows panosuna yazmaz, ses/mikrofon durumunu değiştirmez.

Fiziksel cihaz bağlama/çıkarma, gerçek düşük pil, mikrofon düğmesi ve çıkış değiştirmenin canlı sonucu ayrıca kullanıcı oturumunda denenmelidir. Otomatik görüntü testleri donanım etkileşiminin yerine geçmez.

Çıktı: `out/build/smart-x64/Release/NativeIsland.exe`. Eski sürümü tepsiden Çıkış ile kapatın; sonra güncel EXE'yi veya kurucuyu açın.
