# Tek dosyalık NativeIsland kurulumu

Dağıtılacak tek dosya: installer/Output/NativeIsland-Kurulum.exe.

Bu dosya imzalı MSIX paketini, yalnızca açık anahtarı içeren .cer sertifikasını ve kurulum yardımcılarını kendi içinde taşır. Ayrı MSIX/CER/PowerShell dosyaları göndermek gerekmez; internetten dosya indirmez. Özel imzalama anahtarı içermez.

## Kullanıcı akışı

1. EXE'yi normal çift tıklayarak açın.
2. NativeIsland Local Development sertifikasına güvenme ve paket kurulum onayını verin.
3. Sertifika henüz güvenilir değilse Windows yönetici onayı ister. Yalnızca LocalMachine TrustedPeople deposuna bu sertifika eklenir; Trusted Root, güvenlik politikaları ve bildirim izinleri değiştirilmez.
4. MSIX, kurulumu başlatan normal kullanıcı hesabına yüklenir. Yönetici onayı için farklı bir hesap kullanılsa bile paket o yöneticiye kurulmaz.
5. NativeIsland'ı açın. Ayarlar → Bildirimler üzerinden Windows bildirim erişimini ayrıca onaylayın.

EXE yerel dağıtım için henüz genel güvenilir kod imzalama sertifikasıyla imzalanmamıştır; Windows yayıncı/SmartScreen uyarısı gösterebilir. Kurulum bu güvenlik uyarılarını devre dışı bırakmaz.

## Güvenlik ve güncelleme

Gömülü MSIX SHA256 değeri ve sertifika parmak izi derleme sırasında kurulum koduna sabitlenir. Paketin kurulmadan önce dosya bütünlüğü doğrulanır; MSIX imzası ayrıca Windows tarafından kontrol edilir. Sertifika yayıncısı, parmak izi ve geçerlilik tarihleri doğrulanır.

Aynı veya daha yeni paket zaten kuruluysa sürüm düşürülmez ve tekrar kurulmaz. Daha eski sürüm güncellenir. Açık NativeIsland güncellemeyi engelliyorsa kullanıcıya uygulamayı tepsiden kapatıp tekrar denemesi söylenir; süreçler zorla sonlandırılmaz.

Kurulum ayar, hatırlatıcı veya Google dosyalarını okumaz/yazmaz/silmez. Paket Microsoft uygulama listesine kaydolur; kaldırma Windows Ayarları → Uygulamalar üzerinden yapılır.

## Derleme ve doğrulama

Önce Build-NotificationPackage.ps1 ile imzalı MSIX/CER üretilir. Ardından installer/Build-Bootstrap.ps1 çalıştırılır. Sürüm, SHA256 ve sertifika parmak izi gömülü dosyalardan alınır.

Bu bilgisayarda salt doğrulama modu ve gerçek yeniden kurulum akışı başarıyla çalıştı. Gerçek test aynı/yeni sürümü algılayıp çıkış kodu 0 döndürdü; settings.json ve reminders.json dosyalarının test öncesi/sonrası SHA256 değerleri aynı kaldı. Loglar: out/bootstrap-verify.log ve out/bootstrap-install.log.

Temiz ikinci bilgisayarda ilk kurulum/UAC dalı ayrıca test edilmedi. Windows bildirim izni ve canlı WhatsApp testi kurulumdan bağımsız olarak kullanıcı onayı gerektirir.

Test seçenekleri:

- /VERIFYONLY /VERYSILENT: Gömülü paket doğrulanır; sertifika veya paket kurulmaz.
- /ACCEPTCERTIFICATE /VERYSILENT: Açık komut satırı onayıyla sessiz kurulum. Sertifika güveni eksikse Windows yönetici onayı yine gerekir.
- Onaysız sessiz kurulum reddedilir.
