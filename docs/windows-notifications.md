# Windows bildirimlerini adada gösterme — 0.7.8

## Kullanım

Başlat menüsündeki NativeIsland paketini açın. Eski taşınabilir EXE aynı anda açıksa tepsiden kapatın.

Ayarlar → Bildirimler → Windows bildirimlerini adada göster seçeneğini açın. Erişim izni ver düğmesini kullanıp Windows izin ekranını onaylayın. Ayarlardaki durum satırı erişimin başarılı olup olmadığını gösterir. İzin reddedilirse Windows uygulama gizlilik ayarlarından tekrar verilmesi gerekir.

Herhangi bir uygulamadan Windows bildirim merkezine ulaşan yeni bildirim geldiğinde ada içeriğe göre 336–500 DIP genişliğinde kompakt kart görünümüne geçer; kısa bildirimde gereksiz boşluk bırakmadan, uzun mesajda iki veya üç satırlık önizleme alanı kullanarak Windows'un bildirdiği kaynak uygulamayı, bildirim başlığını ve varsa mesaj önizlemesini ayrı satırlarda gösterir ve 8 saniye sonra eski boyutuna döner. Sıradaki bildirim gösterilmeden önce adadaki diğer kısa bildirimlerin bitmesi beklenir. Ada açıksa içerik alt bildirim satırına gelir; sekme değiştirilmez.

Bildirim kaynağı Windows'un verdiği uygulama adıdır: Chrome/Vivaldi/Edge üzerinden gelen web bildirimleri otomatik olarak WhatsApp veya başka bir hizmet diye sınıflandırılmaz. Windows uygulama kimliği izin verdiğinde gerçek uygulama logosu asenkron olarak RAM'e yüklenir. Klasik Win32 uygulamalarında AUMID boş olsa bile uygulama adıyla önbellekleme yapılır; WinRT logo boş veya çözümlenemez biçimde dönerse HKCR AppUserModelId IconUri, Shell AppsFolder ve yalnızca eşleşen çalışan EXE yolu denenir; 5 dakikalık aralıklı tekrar deneme uygulanır; bunlar da başarısızsa kaynak baş harfiyle nötr rozet gösterilir. Windows UserNotification/NotificationBinding dinleyicisi ayrı profil fotoğrafı öğelerini güvenilir biçimde sunmadığından kişiye ait avatar bu sürümde otomatik alınmaz; kompakt kartta ikinci bir genel çan simgesi çizilmez. Damla ve kapsül ayarlarında aynı okunabilir yerleşim kullanılır. Bildirim metinleri 12/15/13 DIP boyutunda yüksek kontrastla çizilir ve bildirim boyunca opaklık tamdır. Bildirimde desteklenmeyen Yanıtla, Ertele veya Aç eylemleri gösterilmez. Windows bildirimleri silinmez, uygulamalar açılmaz, mesajlara cevap gönderilmez. Windows bildirim merkezine ulaşmayan uygulama-içi mesajlar okunamaz. Başlangıçtaki eski bildirimler tekrar gösterilmez. Windows'ta kaldırılan bildirimler kuyruktan çıkarılır.

## Gizlilik

Özellik varsayılan olarak kapalıdır; açık kullanıcı izni gerektirir. Mesaj içerikleri, bildirim geçmişi veya bildirim kimlikleri dosyaya/loga kaydedilmez. Mesaj başlığını ve içeriğini göster seçeneği kapatılınca yalnızca uygulama adı görünür. En fazla 20 yeni bildirim geçici bellekte sıraya alınır. Kilitli masaüstünde erişim ve gösterim durdurulur; gizli/tam ekran durumundaki bildirimler biriktirilmez.

## Paket ve veriler

İmzalı paket: installer/Output/NativeIsland-Notifications.msix. Yayıncı CN=NativeIsland Local Development. Yerel test sertifikası kullanıcı onayıyla yalnızca LocalMachine TrustedPeople deposuna eklenmiştir; Trusted Root değiştirilmez. Özel anahtar dışa aktarılmaz, CurrentUser My deposunda kalır. Sertifika parmak izi: 49A90A148ADACEAB4CDFCE36598B200398F00230.

Paket userNotificationListener, runFullTrust ve allowElevation yeteneklerini tanımlar. Dosya/registry sanallaştırması kapalıdır: mevcut LOCALAPPDATA/NativeIsland ayar, hatırlatıcı ve Google dosyaları aynı yerde kalır. Bu nedenle paket kaldırılması da bu kullanıcı verilerini temizlemez. Windows başlangıç kaydı paketli sürümde sürüme bağlı EXE yolu yerine kalıcı paket aile kimliğini kullanır.

Build-NotificationPackage.ps1 paketi üretir/imzalar. Install-NotificationPackage.ps1 yalnızca beklenen yayıncı/parmak izini doğruladıktan sonra sertifikayı güvenilen kişilere ekler ve paketi kurar. Sertifikaya güvenme veya paket kurma otomatik yapılmaz; kullanıcı onayı ve UAC gerekir.

## Doğrulama sınırı

Release/core testleri: ilk geçmişin atlanması, yeni kayıtların sıralanması, tekrar göstermeme, silinme ve kimlik yeniden kullanımı, gizli içerik, metin sınırı, ayar okuma. Native önizleme: out/previews/notice-windows.png ve settings-4.png. Native regresyon: bildirim boyutuna geçiş ve süre dolunca eski boyuta dönüş.

MSIX şeması MakeAppx tarafından doğrulandı, paket imzalandı ve bu bilgisayara kuruldu. Canlı WhatsApp uçtan uca testi, Windows bildirim izni verilmesi ve yeni bir mesaj gelmesini gerektirir; bu adım henüz doğrulanmış değildir.

[Microsoft bildirim okuyucusu belgesi](https://learn.microsoft.com/en-us/windows/apps/develop/notifications/app-notifications/notification-listener)
