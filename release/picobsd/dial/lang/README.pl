1998.07.23, Warszawa

			PicoBSD @VER@ (wersja DIALUP)
			---------------------------

Co to jest PicoBSD?
-------------------

Jest to jednodyskietkowa wersja FreeBSD skonfigurowana g³ównie pod k±tem
zastosowania jako narzêdzie dostêpu przez dialup lub ethernet.
W celu zapoznania siê z pe³nym systemem zajrzyj na http://www.freebsd.org

Jakie s± minimalne wymagania?
-----------------------------

* Procesor 386SX lub lepszy (dostêpny jest emulator FPU)
* 8MB pamiêci - jest to absolutnie nieprzekraczalne minimum. Oczywi¶cie im
  wiecej, tym lepiej - ograniczenie jest g³ównie spowodowane brakiem swapu. Po
  zapoznaniu siê z systemem mo¿esz sobie skonfigurowaæ tzw. swap-file na dysku
  twardym, np. na partycji DOS-owej lub Linux-owej. S³u¿y do tego program
  vnconfig, oraz urz±dzenie vn(4). Wówczas prawdopodobnie wystarczy 4MB pamiêci.
* Modem, skonfigurowany na COM1-COM4 (standardowo system wykorzystuje COM2),
  je¶li bêdzie wykorzystywany dostêp przez PPP.
* Karta sieciowa: kompatybilna z NE2000, niektóre typy 3Com, lub wersje PCI z
  chipsetem DEC21040 (drivery ed, ep i de), je¶li bêdziesz korzystaæ z dostêpu
  przez ethernet. Jest te¿ driver do karty PCI Intel EtherExpress (fxp), i 
  kart Lance/PCnet (lnc).

W jaki sposób uzyskaæ dostêp dialup?
------------------------------------

Zalecam skorzystanie ze skryptu /stand/dialup, który skonfiguruje dodatkowo
us³ugê PPP w ten sposób, ¿e bêdzie siê automatycznie ³±czyæ z providerem, oraz
ppp bêdzie dzia³aæ w tle. Je¶li jednak co¶ nie wyjdzie (daj mi znaæ o tym!),
lub lubisz robiæ to sam, oto opis poszczególnych kroków:

1.	wejd¼ do katalogu /etc/ppp i w pliku ppp.conf zmieñ port
	szeregowy na ten, na którym masz modem (cuaa0==COM1, cuaa1==COM2,
	itd...) Mo¿esz to zrobiæ edytorem 'ee /etc/ppp/ppp.conf'.

2.	uruchom program 'ppp'. Przejd¼ do trybu terminalowego (polecenie 
	'term').  W tym momencie masz bezpo¶redni kontakt z modemem, wiêc
	normalnie wybierz numer dialup i zaloguj siê do serwera
	komunikacyjnego. Wydaj mu polecenie przej¶cia w tryb ppp. Powiniene¶
	zobaczyæ co¶ takiego:

	(communication server...): ppp

	ppp on pico> Packet mode
	PPP on pico>

  	W tym momencie jeste¶ ju¿ online! Gratulujê.
3.	Do Twojej dyspozycji s± nastêpuj±ce programy: telnet, ftp, i ssh.
  	Poniewa¿ wywo³a³e¶ 'ppp' rêcznie, wiêc blokuje Ci konsolê. Nie
	szkodzi - masz do dyspozycji 9 kolejnych konsoli wirtualnych, po
	których mo¿na siê poruszaæ naciskaj±c lewy Alt i klawisz funkcyjny
	F1-F10.

Jak skonfigurowaæ kartê Ethernet?
---------------------------------

Miejmy nadziejê, ¿e Twoja karta jest obs³ugiwana przez j±dro dostêpne na
dyskietce, oraz ¿e poprawnie ustawi³e¶ jej parametry (w przypadku kart
ISA) w edytorze UserConfig. Mo¿esz sprawdziæ, czy PicoBSD wykry³o tê kartê,
patrz±c na komunikaty startowe ('dmesg | more').

Naj³atwiejszym sposobem na skonfigurowanie dostepu LAN jest ustawienie
parametrów w pliku konfiguracyjnym systemu ('ee /etc/rc.conf'). Znajd¼ liniê,
która zaczyna siê od 'network_interfaces' i dopisz nazwê sterownika karty do
listy interfejsów. Potem dodaj jeszcze jedn± liniê 'ifconfig_<nazwa>',
która ustawi w³a¶ciwy adres IP i maskê sieci. Np.:

	network_interfaces="lo0 ed0"
	ifconfig_lo0="inet 127.0.0.1"
	ifconfig_ed0="inet 192.168.0.1 netmask 255.255.255.0"

Nastêpnie musisz ustawiæ swój domy¶lny router (jest to zazwyczaj adres IP
routera w Twojej sieci LAN). Np.:

	defaultrouter="192.168.0.100"

Nastêpnie zachowujesz te informacje na dyskietce startowej przy pomocy
polecenia 'update', i restartujesz system.

Oczywi¶cie mo¿na te¿ zrobiæ to rêcznie, wydaj±c odpowiednie polecenia:

	ifconfig ed0 inet 192.168.0.1 netmask 255.255.255.0
	route add default 192.168.0.100

Je¶li poprawnie wszystko ustawi³e¶, powiniene¶ byæ w stanie uzyskaæ
odpowied¼ od swojego routera ('ping 192.168.0.100') oraz od jakiej¶
maszyny poza Twoj± sieci±.

Skad wzi±æ dodatkowe informacje?
--------------------------------

Oficjalna strona projektu PicoBSD:

	http://www.freebsd.org/~picobsd/

Mo¿na tam znale¼æ trochê wiêcej informacji, oraz poprawki i nowe wersje.

Mi³ej zabawy!
  
Andrzej Bia³ecki <abial@freebsd.org>

$FreeBSD$
