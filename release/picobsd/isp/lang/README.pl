1998.07.29, Warszawa

				PicoBSD @VER@
				-----------

Co to jest PicoBSD?
-------------------

Jest to jednodyskietkowa wersja FreeBSD skonfigurowana glownie pod katem
zastosowania jako klient/serwer uslug sieciowych (takich jak routing,
firewall, NFS). W celu zapoznania sie z pelnym systemem zajrzyj na
http://www.freebsd.org

Jakie sa minimalne wymagania?
-----------------------------

* Procesor 386SX lub lepszy (jadro posiada emulator FPU)
* 10MB pamieci - jest to absolutnie nieprzekraczalne minimum. Oczywiscie im
  wiecej, tym lepiej - ograniczenie jest glownie spowodowane brakiem swapu. Po
  zapoznaniu sie z systemem mozesz sobie skonfigurowac tzw. swap-file na dysku
  twardym, np. na partycji DOS-owej. Wowczas prawdopodobnie wystarczy 6MB
  pamieci.
* Modem, skonfigurowany na COM1-COM4 (standardowo system wykorzystuje COM2),
  jesli bedzie wykorzystywany dostep przez PPP.
* Karta sieciowa: kompatybilna z NE2000, niektore typy 3Com, lub wersje PCI z
  chipsetem DEC21040 (drivery ed i de). Jadro jest skonfigurowane tak, zeby
  moc obsluzyc po dwie karty ed i de (czyli w sumie cztery) oraz dwa
  polaczenia PPP rownoczesnie. Mozna wiec zbudowac router z 6 interfejsami.


Milej zabawy!
  
Andrzej Bialecki <abial@freebsd.org>
