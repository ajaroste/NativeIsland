# NativeIsland 0.8 — Damla, ses mikseri ve Google Tasks

## Kullanım

Ana ses ayrı vurgu rengi ve çerçeveli karttır. Uygulama satırlarının yıldızına tıklayarak favori seçilir/kaldırılır. Favori uygulamanın ses oturumu varsa başa alınır; kapalı uygulamalar listeye eklenmez. Favoriler kullanıcı klasöründeki `audio-favorites.json` dosyasında tutulur ve yeniden başlatma/güncellemelerde korunur. Kimliği okunamayan korumalı ses oturumlarında yıldız devre dışıdır. 0.8.4 ile dışarıdaki yan düğme kaldırıldı; görevler ana panelde çanın sağındaki liste ikonundan açılır. Bağlantı Ayarlar → Google Tasks üzerinden yapılır.

- Ayarlar → Görünüm → Ada biçimi: Kapsül veya Damla. Damla seçilince üst boşluk görsel olarak sıfırlanır; kapsüle dönünce eski boşluk ayarı geri kullanılır.
- Çan yanındaki hoparlör: ana ses, uygulama ses oturumları, sessize alma, çıkış seçimi ve mikrofon. Medya sayfası yalnızca oynatıcıdır. Tekerlekle daha fazla ses oturumu görülebilir. Windows yalnızca ses oturumu oluşturmuş uygulamaları sağlar; ayrı ses oturumları ayrı satırlar olarak gösterilir.
- Çanın sağındaki liste ikonu: görev sayfasını açar. Tekrar Medya veya başka bir sekmeye tıklayarak geri dönülür. Google bağlantısı yoksa bağlantı yönergesi gösterilir. Bekleyen görev varsa ikon üzerinde küçük vurgu noktası görünür.
- Görevlerde üstteki liste alanından liste seç; + ile yeni görev alanını aç ve yalnızca Ekle düğmesiyle gönder. Yazmak, metin alanında Enter veya formu kapatmak gönderim yapmaz. Onay kutusuyla tamamla; tamamlanan görevler listede ve eski önbellekte gizlenir. Sağdaki × simgesi, onaydan sonra görevi Google Tasks hesabından siler. Eşitle simgesiyle yenile. Çevrimdışıyken son eşitlenen liste korunur, başarısız gönderimler otomatik tekrarlanmaz.

## Google kurulumu

1. Takvim için kullandığın Google Cloud projesinde [Google Tasks API](https://console.cloud.google.com/apis/library/tasks.googleapis.com) etkinleştir.
2. Gerekirse Google Auth Platform → Data Access içinde `https://www.googleapis.com/auth/tasks` kapsamını ekle. Test modundaysan hesabının test kullanıcısı olduğundan emin ol.
3. NativeIsland → Ayarlar → Google Tasks → Tasks izni ver. Mevcut OAuth Masaüstü istemcisi kullanılır. Henüz Takvim bağlantın yoksa aynı sayfadaki JSON ile bağla seçeneğini kullan.
4. Tarayıcıda Takvim okuma ve Tasks görev yönetimi izinlerini onayla. NativeIsland görevleri listeleyebilir, ekleyebilir, tamamlayabilir ve kullanıcı onayından sonra silebilir. Takvim salt okunurdur.
5. Görev listesini seç. Görevler yaklaşık 5 dakikada bir veya elle eşitlenir.

JSON dosyan yoksa Google Auth Platform → Clients → Create client → Desktop app üzerinden OAuth istemcisi oluştur; istemcinin JSON dosyasını indir ve NativeIsland → Ayarlar → Google Tasks → JSON ile bağla ile seç. API anahtarı veya servis hesabı bu bağlantının yerine geçmez. JSON dosyasını ve erişim belirteçlerini herkese açık paylaşma.

Mevcut sadece-Takvim izni kendiliğinden genişletilmez. Kullanıcı onayı gerekir. Yeniden JSON indirmek gerekmez; aynı Masaüstü OAuth istemcisi kullanılabilir.

Bağlantı, liste seçimi ve son görev önbelleği `%LOCALAPPDATA%\NativeIsland\google-calendar.dat` içinde Windows DPAPI ile şifrelenir. Güncelleme bu dosyayı, ayarları veya hatırlatıcıları silmez. Google Takvim sayfasında bağlantıyı kaldırmak iki hizmetin yerel bağlantı verilerini kaldırır; Google hesabındaki görevler silinmez.

## Doğrulama sınırı

Yerel testler JSON ayrıştırma, damla hit-test geometrisi, sekme/rol geçişi, görev giriş formu, ekran görüntüleri ve Windows ses oturumu okumayı doğrular. Google hesabına canlı bağlanma / gerçek görev yazma ve kullanıcının gerçek uygulamalarının sesini değiştirme ayrı kullanıcı izinleri ve canlı test gerektirir; önizleme testleri bunları yapmaz.

API kaynakları: [Tasks insert](https://developers.google.com/workspace/tasks/reference/rest/v1/tasks/insert), [Tasks patch](https://developers.google.com/workspace/tasks/reference/rest/v1/tasks/patch), [Tasks delete](https://developers.google.com/workspace/tasks/reference/rest/v1/tasks/delete), [OAuth kapsamları](https://developers.google.com/workspace/tasks/auth).
