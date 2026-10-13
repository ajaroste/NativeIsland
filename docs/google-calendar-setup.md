# NativeIsland 0.7.4 — Google Takvim bağlantısı

Hatırlatıcılar internet veya Google hesabı olmadan kullanılabilir. Adanın sağ üstündeki çana, ardından **+** düğmesine tıkla. Kısa ekleme alanında başlık, tarih, saat veya "Tüm gün" seçip **Ekle**. Gün başlangıcı varsayılan 08.00; artık **Ayarlar** içinden değiştirilebilir. Günlük özet bilgisayar geç açılırsa ilk açılışta ada içinde gösterilir; Windows'un sağ alt bildirimi kaldırıldı. Uygulama kapalıyken bildirim üretilemez; istersen ayarlardan "Windows başladığında aç" seçeneğini etkinleştir.

## Google Cloud hazırlığı

1. [Google Cloud Console](https://console.cloud.google.com/) içinde kendine ait bir proje oluştur veya seç.
2. APIs & Services → Library içinden **Google Calendar API** etkinleştir.
3. Google Auth Platform / OAuth consent screen ekranında uygulama adı ve iletişim bilgilerini doldur. Kişisel test için External / Testing seçilirse kendi Google hesabını Test users listesine ekle. Kurumsal hesapta yöneticinin politikaları geçerlidir.
4. Clients / Credentials → Create OAuth client ID → **Desktop app / Masaüstü uygulaması** seç. Web application veya service account seçme.
5. Bu istemcinin JSON dosyasını indir. Dosyanın kökünde `installed` alanı bulunmalıdır. Dosyayı veya token dosyalarını herkese açık bir yere yükleme; sohbet içinde paylaşman gerekmez.

Google'ın [masaüstü OAuth yönergesi](https://developers.google.com/identity/protocols/oauth2/native-app), API etkinleştirme, istemci oluşturma ve tarayıcıda kullanıcı izni adımlarını açıklıyor.

## NativeIsland'da bağlanma

1. **Ayarlar → Google Takvim → Google bağla (JSON)…** düğmesine bas ve indirdiğin JSON'u seç. Google bağlantısı, eşitleme, bağlantıyı kaldırma ve takvim seçimi artık Ayarlar'dadır; çan → **+** yalnızca kısa bir yerel hatırlatıcı ekleme alanı açar.
2. Varsayılan tarayıcıda Google hesabını seçip Takvim **okuma** iznini ver. Uygulama Google şifreni istemez.
3. Başarılı yetkilendirme sonrası tarayıcı sekmesini kapatabilirsin. NativeIsland'a dön; durum satırında "Google Takvim güncel · salt okunur" görünmeli.
4. Takvim listesinde Ctrl ile istediğin takvimleri seç, **Seçili takvimleri uygula** düğmesine bas. Başlangıçta Google'da seçili takvimler kullanılır. **Şimdi eşitle** ile yeniden çekebilirsin.

Uygulama her 5 dakikada eşitler; dünden başlayarak yaklaşık bir sonraki haftanın kayıtlarını önbelleğe alır. Çan ekranında Dün / Bugün / Yarın sekmeleri bulunur; tamamlanan kayıtlar da durum işaretiyle görünür. Günlük bildirimler bugünün tamamlanmamış kayıtlarına göre çalışır. Tekrarlanan etkinlikler, API'nin `singleEvents` seçeneğiyle ayrı oluşumlar olarak alınır. Google etkinlikleri yaratılmaz, değiştirilmez veya silinmez. "Tamamla" yalnızca NativeIsland'daki görünümü etkiler. [Etkinlik listeleme API'si](https://developers.google.com/workspace/calendar/api/v3/reference/events/list).

## Kayıtların korunması

- `%LOCALAPPDATA%\NativeIsland\settings.json`: mevcut ayarlar ve günlük bildirim tercihleri.
- `%LOCALAPPDATA%\NativeIsland\reminders.json`: yerel kayıtlar, Google önbelleği, tamamlanma ve son bildirim durumu.
- `%LOCALAPPDATA%\NativeIsland\google-calendar.dat`: OAuth istemci ve yenileme token bilgileri; Windows DPAPI ile mevcut kullanıcı için şifreli.
- `.bak` dosyaları bir önceki kayıt yedeğidir. `.tmp` dosyaları atomik kayıt sırasında kullanılır.

Bu klasör kurulum klasöründen ayrıdır. Güncelleme veya normal kaldırma bu dosyaları silmez. "Varsayılanlara dön" hatırlatıcıları veya Google bağlantısını silmez. "Bağlantıyı kaldır" yalnızca Google bağlantısını ve Google önbelleğini kaldırır; yerel hatırlatıcılar kalır. Hesabın Google tarafındaki uygulama izni otomatik iptal edilmez; istersen Google hesap güvenliği ekranından kaldırabilirsin.

Bozuk hatırlatıcı dosyasında geçerli yedek varsa kurtarma yapılır. Yedek de okunamazsa mevcut dosyanın üzerine yazılmaz. Daha yeni hatırlatıcı şeması da korunur. DPAPI dosyası başka Windows kullanıcısına taşınarak kullanılamaz; o kullanıcı yeniden bağlanmalıdır.

## Hata durumları ve doğrulama sınırı

JSON yoksa veya kullanıcı izin vermezse Google eşitlemesi tamamlanamaz. API kapalı, test kullanıcısı eksik, kurumsal kısıtlama veya yenileme izni sona ermişse durum satırındaki açıklamayı kontrol et. Ağ/Google hatasında önceki kayıtlar korunur. İlk bağlantı 3 dakika sonra zaman aşımına uğrar. Tekrar bağlanma düğmesiyle yeniden dene.

Yerel kayıt, yeniden başlatma/güncelleme korunması, günlük/saatlik bildirim tekrar engelleme, yedek kurtarma, yazma hatasında geri alma, gelecekteki şemayı koruma, RFC3339 saat dilimleri, PKCE test vektörü ve Google yanıt örnekleri otomatik test kapsamındadır. **Gerçek hesapta OAuth giriş/yenileme ve Google eşitlemesi, henüz istemci JSON'u olmadığı için canlı doğrulanmadı.** Windows Odaklanma/Rahatsız Etmeyin ve sistem bildirim tercihleri tepsi bildirimini bastırabilir; çan ekranındaki kayıtlar yine görülebilir.
