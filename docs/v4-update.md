# NativeIsland 0.4.0

## 0.4.1 ayar güncellemesi

Ayarlarda **Windows başladığında aç** başlangıç kaydını açar/kapatır. Kullanıcı seçimi olmadan etkinleştirilmez; mevcut tercih korunur. Otomatik başlangıç da normal başlangıcın UAC onayını kullanır; sessiz veya UAC'siz yükseltme yapılmaz. Gerçek yeniden oturum açma testi otomatik doğrulama kapsamına dahil değildir.

**Tıklayınca açık tut** varsayılan olarak açıktır. Kapatıldığında tıklama, tepsi üzerinden açma veya dosya bırakma kalıcı sabitleme oluşturmaz. Fare ayrılınca küçülmesi için ayrıca **Fare ayrılınca küçült** açık olmalıdır. Daha önce sabitlenmiş ada da bu seçenek kapatıldığında normal küçülme davranışına döner.

Kısayol yanındaki **Windows tuşu**, seçilen kombinasyona klavyedeki Windows logo tuşunu ekler; başlangıç ayarıyla ilgili değildir.

Bu sürümün çıktısı `out/build/v4-settings-x64/Release/NativeIsland.exe` içindedir.

## Kullanım

- Pano izlemeyi Pano sekmesinden etkinleştirin. Metin veya görsel satırına tıklamak ayrı bir önizleme penceresi açar. Metin önizlemesi seçilebilir, kaydırılabilir ve yeniden boyutlandırılabilir. Sağdaki pano simgesi içeriği yeniden kopyalar.
- Geçmiş yalnızca RAM'dedir: en fazla 20 kayıt / 64 MiB. Tek görselin DIB verisi en fazla 8 MiB, çözülmüş görüntü en fazla 4 megapiksel; daha büyük girdiler alınmaz. Metin güvenli okuma sınırı 1 milyon UTF-16 karakterdir. DIB/DIBV5 ve bitmap pano biçimleri desteklenir.
- Uygulamalar dört sütunlu kutulardır. En fazla iki satır görünür; tekerlek dört kutuluk satırlarla kaydırır. + ekler, kutuya tıklama başlatır, x yalnızca kısayolu listeden kaldırır.
- Dosya rafı Windows'un gerçek dosya/klasör/ilişkili uygulama simgesini gösterir. Kaynak dosyalar taşınmaz veya silinmez.
- Ada zeminini sürükleme yalnızca yatay konumu değiştirir; üst kenar ve en üstte kalma korunur. Konum yüzde olarak kaydedilir ve geniş panelin ekran sınırlarında kalması sağlanır.
- Ayarlar: ana/ek monitör seçimi, Sol / Ortala / Sağ, gizle-göster kısayolu. Varsayılan Ctrl+Alt+I. Kısayol alanına kombinasyonu basın; Win eklemek için ayrı kutuyu işaretleyin. Çakışan kısayol uyarı verir. Boş kısayol devre dışıdır.
- Gizle adayı tamamen saklar, uygulamadan çıkmaz. Sistem tepsisi simgesi görünür kalır; simgeye tıklamak tekrar gösterir. Sağ tık Ayarlar'a erişir.

## Yönetici başlangıcı ve tek örnek

Normal başlangıç UAC ile yükseltilmiş bir denetleyici açar. Denetleyici Explorer otomasyonu üzerinden **normal kullanıcı yetkisinde** tek masaüstü arayüzü açar ve arayüz kapanınca çıkar. Bu tasarım Explorer → ada OLE sürükleme/kopyalama işlemlerinin Windows UIPI tarafından engellenmesini önler. Görsel arayüzün kendisi yönetici değildir. Denetleyici ayrı bir uygulama örneği değil, başlangıç yardımcısıdır; ayrı bir pencere/sistem tepsisi oluşturmaz ve ayrıcalıklı komut kanalı sunmaz.

Manifest bu nedenle asInvoker kalır; normal giriş noktası runas kullanır. UAC reddedilirse uygulama açılmaz. UAC kapalıysa veya Explorer yükseltilmişse Windows'un ilgili güvenlik davranışı geçerlidir. Desteklenen akış aynı kullanıcının UAC yükseltmesidir; farklı bir yönetici hesabının kimlik bilgileriyle başlangıç bu sürümde doğrulanmamıştır ve oturum mutex izinleri nedeniyle başarısız olabilir.

Tek örnek denetimi oturum bazlı mutex ile hem denetleyici hem arayüz için ayrıdır. Yeniden açma ikinci bir ada oluşturmaz. --desktop-ui yalnızca denetleyici varken çalışabilir; bu bir güvenlik sınırı olarak kullanılmaz. --render-previews ve --smoke-test yalnızca doğrulama için ayrılmış örneklerdir; normal başlangıcın UAC akışını test etmez.

Microsoft'un Explorer üzerinden yükseltilmemiş süreç başlatma yaklaşımı: https://learn.microsoft.com/en-us/windows/win32/shell/samples-execinexplorer

## Yeni sürüm

Çalışan eski EXE kilitlenmiş olabileceğinden yeni çıktı `out/build/v4-x64/Release/NativeIsland.exe` içindedir. Eski adayı sistem tepsisinden **Çıkış** ile kapatın, ardından bu EXE'yi veya güncel kurucuyu kullanın.

## Doğrulama kapsamı

12.10.2026 sonuçları: Release derlemesi başarılı; CTest 1/1 geçti; 36 tema/sayfa PNG'si oluşturuldu; pencere geometrisi, hizalama ve gizle/göster raporu başarılı; 30 saniyelik izole çalışma testi çıkış kodu 0. Güncel EXE ile Inno Setup kurucusu üretildi.

Core testleri DIB çözümleme, bozuk görüntü reddi, uzun metin veri modeli, dört sütunlu sayfa yüksekliği ve yeni ayar ayrıştırmasını kapsar. Yerleşik PNG dışa aktarımı gerçek uygulama simgelerini ve izole pano görüntüsü/metni test girdilerini çizer. Bu test girdileri kullanıcının panosuna veya kayıtlı kısayollarına yazılmaz.

UAC onayıyla tam başlangıç, gerçek Explorer sürüklemesi, çok monitör üzerinde DPI geçişi ve klavye kısayolunun gerçek fiziksel girdisi ayrıca kullanıcı oturumunda denenmelidir. Otomatik testler bunların yerine geçmez.
