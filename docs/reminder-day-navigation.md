# NativeIsland 0.7.6 — Gün geçişi

- Çan ekranındaki Dün / Bugün / Yarın sekmeleri seçili günün kayıtlarını gösterir.
- Tamamlanmış kayıtlar durum işaretiyle görünür.
- + ve Hatırlatıcı ekle, seçili günün tarihini önerir. Diğer ekranlardan ekleme bugünü önerir.
- Gün değişiminde liste başa döner ve kısa bir geçiş uygulanır. Hareketi azalt açıkken geçiş anlıktır.
- Günlük özet, çan rozeti ve zamanlı bildirimler bugüne göre çalışır.
- Google eşitlemesi dünü de kapsar. Eski önbellekte bulunmayan kayıtlar sonraki eşitlemede gelir; ayarlardan elle de eşitlenebilir.
- Mevcut ayar, hatırlatıcı ve Google bağlantı dosyalarının konumu/şeması değiştirilmedi.

Release derlemesi ve core testleri başarılı. Tarih sınırları, tamamlanan kayıtlar, artık yıl ve yıl geçişleri test edildi. Native arayüz testinde üç sekme, seçim/scroll sıfırlaması ve seçili yarın tarihine kayıt oluşturma doğrulandı. Önizlemeler out/previews/reminders-day-0.png, reminders-day-1.png, reminders-day-2.png ve reminders-days-empty.png; rapor out/previews/verification.txt.

Canlı Google hesabı yeniden test edilmedi; mevcut salt okunur bağlantı akışı korunmuştur.
