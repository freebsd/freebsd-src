1998.07.12, Warszawa

			PicoBSD @VER@ (wersja ROUTER)
			-------------------------

Co to jest PicoBSD?
-------------------

Jest to jednodyskietkowa wersja FreeBSD skonfigurowana pod katem
zastosowania jako router, oferuje rowniez modul IP Firewall i translacje
adresow (NAT).

W celu zapoznania sie z pelnym systemem zajrzyj na http://www.freebsd.org.
Oficjalna strona tego projektu znajduje sie na

		http://www.freebsd.org/~picobsd.

Jakie sa minimalne wymagania?
-----------------------------

* Procesor 386SX lub lepszy (jadro posiada emulator FPU)
* 4MB pamieci - jest to absolutnie nieprzekraczalne minimum. Oczywiscie im
  wiecej, tym lepiej - przy tej ilosci mozliwe jest skonfigurowanie
  statycznego routingu oraz IP Firewalla; jesli potrzebujesz rowniez
  daemona routed i translacji adresow, wymagane jest minimum 6MB.
* Modem, skonfigurowany na COM1-COM4 (standardowo system wykorzystuje COM2),
  jesli bedzie wykorzystywany dostep przez PPP.
* Karta sieciowa: kompatybilna z NE2000, niektore typy 3Com, lub wersje PCI z
  chipsetem DEC21040 (drivery ed, ep, fxp i de). Jadro jest skonfigurowane
  tak, zeby moc obsluzyc po dwie karty ed, ep, i de (czyli w sumie siedem)
  oraz dwa polaczenia PPP rownoczesnie. Mozna wiec zbudowac router z 9
  interfejsami... :-)

Po dalsze szczegoly zajrzyj do oryginalnej dokumentacji.


Milej zabawy!
  
Andrzej Bialecki <abial@freebsd.org>
