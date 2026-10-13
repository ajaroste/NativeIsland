# NativeIsland 0.5.0 — animasyon

Ayarlardaki Açılış animasyonu:

- Yumuşak (varsayılan): sıfır başlangıç/bitiş hızına ve ivmesine sahip quintic geçiş; boyut taşması yok.
- Yaylı: küçük, sınırlı hedef aşımı ile esneyerek oturma.
- Hızlı: seçilen sürenin %65'inde ease-out geçiş.
- Animasyonsuz: hedef boyuta doğrudan geçiş.

Animasyon süresi 120–600 ms aralığındadır; varsayılan 300 ms. Açılış ve kapanış aynı seçeneği kullanır. Azaltılmış hareket tercihi her türü geçersiz kılar ve anında geçiş uygular. Eski animasyonSpeed alanı geriye dönük olarak korunur; yeni süre ayarı artık bu alanı kullanmaz.

## Altyapı

Swap-chain tamponu en az 720 × 560 DIP ayrılır. Animasyon sırasında HWND ve görünür kırpma değişir; GPU tampon boyutu sabit kalır. Kaynaklar yalnızca DPI değişirse, cihaz kaybolursa veya kapasite yetersizse yeniden oluşturulur. PNG dışa aktarımı kapasite yerine görünür alanı kopyalar.

DXGI frame-latency waitable object ve maksimum bir karelik sunum kuyruğu kullanılır. Mesaj döngüsü animasyon aktifken ekran sunum sinyali veya kullanıcı mesajını bekler; animasyon bitince yalnızca mesaj beklemeye döner. Desteklenmeyen durumda 16 ms zamanlayıcı yedeği vardır. Kullanıcı mesajları animasyon karesinde birleştirilir. Bu yaklaşım sabit bir FPS garantisi değildir; sürücü, ekran ve sistem yükü hâlâ etkilidir.

İçerik final panel genişliğinde yerleşir; büyüyen yuvarlak maske ve yumuşak opaklık geçişiyle açılır. Yarıda yön değiştirme mevcut boyuttan başlar; hedef boyuta sıçrama yapılmaz. Yaylı mod fiziksel hız koruyan bir yay simülasyonu değil, sınırlı esnemeli bir easing eğrisidir.

Kaynak: [Microsoft DXGI frame-latency rehberi](https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains).

## Testler

12.10.2026: Release derlemesi ve CTest 1/1 başarılı. Yerleşik testte DXGI kare bekleme sinyali mevcut; dört mod, yarıda tersine çevirme ve sıfır tampon yeniden tahsisi kontrolleri geçti. Altı temada 36 statik önizleme üretildi. 30 saniyelik çalışma testi çıkış kodu 0; yeni EXE ile kurucu oluşturuldu.

Eğrilerin uç noktaları, sınırları, yumuşak/hızlı mod monotonluğu ve ayar sınırları core testindedir. Yerleşik görsel test dört modda aç/kapat, yarıda tersine çevirme, ilk pencere boyutuna dönüş ve animasyon boyunca sıfır tampon yeniden tahsisini doğrular. Algılanan akıcılık ve farklı ekran yenileme hızları ayrıca gerçek kullanımda değerlendirilmelidir.

Yeni çıktı: `out/build/animation-x64/Release/NativeIsland.exe`. Önce eski adayı sistem tepsisinden kapatın.
