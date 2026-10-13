# NativeIsland 0.8.7 — ilk public yayın

Windows 10 2004+ / Windows 11 x64 için NativeIsland topluluk sürümü.

## Kurulum

Bu sürümün eklerindeki **NativeIsland-Kurulum.exe** dosyasını indir ve normal çift tıklayarak aç. Eski sürüm açıksa önce sistem tepsisinden Çıkış seç. MSIX ve açık sertifika kurulumun içinde bulunur; ayrı dosya indirmek gerekmez. Visual C++ runtime ayrıca kurulmaz.

**Güvenlik notu:** Kurulum EXE'si genel güvenilir bir yayıncı sertifikasıyla imzalanmış değildir. Windows yayıncı/SmartScreen uyarısı gösterebilir. MSIX, **NativeIsland Local Development** sertifikasıyla imzalıdır. Kurulum bu sertifikayı yalnızca açık onayınla TrustedPeople deposuna ekler; Windows yönetici onayı gerekebilir. Trusted Root ve güvenlik politikaları değiştirilmez. Özel imzalama anahtarı dağıtılmaz. Güvenlik korumalarını kapatma.

Dosya bütünlüğü için aynı sürümün **SHA256SUMS.txt** ekiyle `Get-FileHash .\NativeIsland-Kurulum.exe -Algorithm SHA256` sonucunu karşılaştır.

## Son değişiklikler

- Tam Uygulamalar etiketi ve dar panelde çakışmayan menü.
- Google Tasks, ayrı yan düğme yerine çanın yanındaki liste ikonunda.
- Tamamlanan görevler gizlenir; eski görev önbelleği de filtrelenir.
- Görev yazarken yanlışlıkla gönderim düzeltildi: yalnızca Ekle düğmesi gönderir.
- Görevlerde onaylı silme. Silme Google Tasks hesabındaki görevi de kaldırır.
- Ana ses ve uygulama sesleri ayrı kartlarda; yıldızlı açık uygulamalar önde.
- Public dağıtım için statik Visual C++ runtime, kaynak MIT lisansı ve paket lisansları.

Ayarlar, hatırlatıcılar ve mevcut Google bağlantısı güncellemede korunur. Windows bildirim erişimi Ayarlar → Bildirimler içinden ayrıca onaylanır. Google bağlantısı isteğe bağlıdır; her kullanıcı kendi Desktop OAuth JSON dosyası ve hesabıyla bağlanır. Ortak Google istemcisi/kişisel token içermez.

Yerel derleme, otomatik testler, native renderer testleri ve kurulum bütünlük kontrolü doğrulanır. Temiz ikinci bilgisayar/donanım çeşitliliği ve gerçek Google/WhatsApp akışlarının tamamı doğrulanmış değildir; sorunları Issues bölümünde bildirebilirsin.
