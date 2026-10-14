# NativeIsland 0.8.11

Windows 10 2004+ / Windows 11 x64 için topluluk sürümü.

## Yenilikler

- Yapılacaklarda silme düğmesinin yanında düzenleme kalemi. Mevcut başlık hazır gelir; yalnızca Kaydet günceller. Mevcut görev kimliği kullanılır; yeni kopya oluşturulmaz, not ve tarih değiştirilmez.
- Aynı anda tek görev formu; tekrarlanan tıklamalar ikinci pencere açmaz veya açık formun kimliğini/metnini değiştirmez. Kaydet sırasında yinelenen gönderim engellenir. Değişiklik yoksa veya form iptal edilirse istek gönderilmez.
- Windows bildirimlerinde yeni kompakt yerleşim, gerçek uygulama ikonunu asenkron yükleme ve Win32 Shell ikon yedekleri.
- Uygulama ikonu uygulama adının yanında küçük gösterilir. Uygulama adı, başlık ve mesaj aynı sol hizadadır; mesaj satır aralığı ve kapsül/damla kenar boşlukları düzenlendi.
- Fotoğraf yoksa büyük avatar dairesi/harf rozeti çizilmez. Ayrı görsel alanı hazırdır; ancak mevcut Windows UserNotificationListener API'si WhatsApp kişi fotoğrafı öğesini sağlamaz. Bu sürüm otomatik WhatsApp profil fotoğrafı aktarımı içermez; tarayıcı/WhatsApp önbelleği veya Windows bildirim veritabanı taranmaz.

## Kurulum / güncelleme

Bu sürümün **NativeIsland-Kurulum.exe** ekini indir. Açık NativeIsland'ı tepsiden Çıkış ile kapatıp kurulumu normal çift tıklayarak çalıştır. Başlat menüsünden NativeIsland'ı aç. Ayarlar, hatırlatıcılar ve Google bağlantısı kurulum klasöründen ayrı tutulur ve güncellemede korunur.

**İmzalama:** MSIX, NativeIsland Local Development sertifikasıyla imzalıdır; kurulum EXE'si genel güvenilir yayıncı imzası taşımaz. Windows yayıncı/SmartScreen uyarısı gösterebilir. Dosyanın bu depodan geldiğini doğrula; güvenlik korumalarını kapatma. Kurulum sertifika güveni için açık onay ve gerektiğinde UAC ister; yalnızca ilgili sertifikayı TrustedPeople deposuna ekler, Trusted Root veya genel güvenlik politikalarını değiştirmez. Özel imzalama anahtarı dağıtılmaz.

İndirme bütünlüğü: **SHA256SUMS.txt** ile `Get-FileHash .\NativeIsland-Kurulum.exe -Algorithm SHA256` sonucunu karşılaştır.

Google Tasks bağlantısı mevcut kullanıcı iznini kullanır; ortak OAuth istemcisi veya kişisel token pakette bulunmaz. Windows bildirim erişimi Ayarlar → Bildirimler bölümünden ayrıca onaylanır.

## Doğrulama sınırı

Yerel Release derlemesi, otomatik testler, native renderer/form testleri ve kurulum bütünlük kontrolü geçti. Görev düzenleme testleri PATCH gövdesini/kimliğini, tek pencereyi, tekrar gönderim engelini, iptal/değişikliksiz durumları doğrular; test amacıyla gerçek Google hesabında görev değiştirilmedi. Vivaldi için bu bilgisayarda Shell ikon verisi alınabildi; LanDeck ve canlı bildirim ikonları her kurulumda garanti edilmez. Gerçek WhatsApp fotoğrafı otomatik alınmaz. Temiz ikinci bilgisayar ve tüm donanım/DPI kombinasyonları doğrulanmış değildir.
