Papagan — Başlangıç Hızlı Kılavuz

Bu dal (papagan/init) Papagan dağıtımının başlangıç iskeletini içerir:
- ports/papagan-meta: Meta-port (papagan-setup script'i ile)
- usr/local/bin/papagan-assistant: Basit HTTP stub daemon
- usr/local/etc/rc.d/papagan_assistant: rc.d servis betiği
- ui/papagan-dock.qml: QtQuick dock prototipi

Hedef: Ben sistemi geliştirip paketleri ve servisleri hazırlıyorum; sen daha sonra bunları kullanarak ISO oluşturacaksın.

Hızlı kullanım (geliştirme ortamı — FreeBSD x86_64 jail/VM içinde):
1) Repo'yu papagan/init dalından çekin veya bu dalı checkout edin.
2) papagan-assistant çalıştırmak için:
   - python3 kurulu olmalı (pkg install python39)
   - chmod +x /usr/local/bin/papagan-assistant
   - /usr/local/bin/papagan-assistant &
   - veya servisi kullan: cp usr/local/etc/rc.d/papagan_assistant /usr/local/etc/rc.d/ && sysrc papagan_assistant_enable=YES && service papagan_assistant start
3) papagan-dock prototipini test etmek için Qt Quick runtime kurun (pkg install qt5) ve bir geliştirici ortamında papagan-dock.qml dosyasını QtCreator veya qmlscene ile açın: qmlscene ui/papagan-dock.qml
4) papagan-meta portu iskeletidir; Poudriere/jail içinde derlemek için ports tree'ine yerleştirin.

Önemli uyarılar ve sınırlamalar:
- macOS (Darwin) bileşenleri telifli ve FreeBSD'ye doğrudan taşınamaz. macOS masaüstü/kodlarını kullanmak yasal olarak mümkün değildir. Papagan masaüstünü sıfırdan veya açık projelerden (Qt, wlroots vb.) yeniden implemente edeceğiz.
- Apple Silicon (M1/M2) üzerinde çalıştırma: FreeBSD arm64 desteği var ancak Apple Silicon Mac'lerde tam donanım uyumluluğu (GPU hızlandırma, bootloader) özel çalışmalar gerektirir. İlk hedef x86_64 (Intel/AMD). ARM64 desteğini daha sonra genişletiriz.
- Lisans: Sen kapalı kaynak dağıtmak istiyorsun. Repo halen GitHub'da; kapalı kaynak dağıtım istiyorsan repo'yu private yapman ve ikili paketleri/ISO'yu kendi dağıtım politikana göre paylaşman gerekir. Ayrıca kullandığımız açık kaynak bağımlılıkların lisanslarına (GPL, MIT, BSD vb.) uyman gerekir.

Bir sonraki adımlar (ben yapacağım):
- papagan-dock için ports/Makefile ve örnek Qt/C++ launcher ekleyeceğim.
- assistant için bir plugin arayüzü ve yerel model adaptörü (ggml/llama.cpp için stub) ekleyeceğim.
- papagan-meta ports paketini tamamlayıp pkg-descr, files dizinini genişleteceğim.
- Sana ISO üretimi için gerekli post-install betikleri ve adım adım komut listesini vereceğim.

Eğer hazırsa, ben şimdi papagan-dock ports iskeleti, assistant plugin ve NOTICE dosyalarını ekliyorum. Bu eklemeler geliştirme dalına (papagan/init) eklenecek.