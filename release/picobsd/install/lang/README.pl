1998.07.12, Warszawa

			PicoBSD @VER@ (wersja NET)
			------------------------

Co to jest PicoBSD?
-------------------

Jest to jednodyskietkowa wersja FreeBSD skonfigurowana glownie pod katem
zastosowania jako klient/serwer uslug sieciowych (takich jak routing,
firewall, NFS). W celu zapoznania sie z pelnym systemem zajrzyj na
http://www.freebsd.org. Oficjalna strona tego projektu znajduje sie na
http://www.freebsd.org/~picobsd.

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
  chipsetem DEC21040 (drivery ed, ep, fxp i de). Jadro jest skonfigurowane
  tak, zeby moc obsluzyc po dwie karty ed, ep, i de (czyli w sumie siedem)
  oraz dwa polaczenia PPP rownoczesnie. Mozna wiec zbudowac router z 9
  interfejsami... :-)

Jakie sa roznice w stosunku do poprzedniej wersji?
--------------------------------------------------

* Poszerzony zestaw sterownikow w jadrze systemu
* dodana obsluga CD-ROM
* agent SNMP (pelna wersja ucd-snmp, pozwalajaca na monitorowanie procesow i
  zdalne uruchamianie skryptow)
* brak ssh, ftp i edytora ee (oznacza to, ze musisz edytowac pliki
  konfiguracyjne montujac dyskietke na normalnym systemie)
* dodany inetd, telnetd, routed, tftpd, bootpd, ps, kill, netstat,
  ping, traceroute
* brak vnconfig i vn(4): w przypadku routera powinien on miec tyle pamieci
  RAM, zeby nie potrzebowac swapu, lub miec normalny swap.
* dodana obsluga hasel (passwd(1))
* dodana obsluga NFS (klient)
* sa dwaj uzytkownicy: root (haslo 'setup') i user (haslo 'PicoBSD'). Ze
  wzgledu na skomplikowana sprawe z prawami dostepu, user praktycznie moze
  jedynie zrobic te rzeczy, ktore nie wymagaja praw roota (czyli np. telnet).
* dodany skrypt 'update', ktory powoduje uaktualnienie zawartosci katalogu
  /etc na dyskietce w stosunku do tego, co znajduje sie na MFS.


Milej zabawy!
  
Andrzej Bialecki <abial@freebsd.org>
