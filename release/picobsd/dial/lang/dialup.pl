#!/bin/sh
# $FreeBSD$
set_resolv() {
	echo "[H[J"
	echo "[1m                       Domy¶lna Nazwa Domeny[m"
	echo ""
	echo "Podaj domy¶ln± nazwê domeny Internetowej, której bêdziesz u¿ywaæ."
	echo "Je¶li Twój provider ma nazwy typu 'www.akuku.com.pl', to bêdzie"
	echo "to najprawdopodobniej 'akuku.com.pl'."
	echo ""
	echo "Je¶li po prostu naci¶niesz Enter, ustawisz (nieistniej±c±) domenê"
	echo "'mydomain.org.pl', co nie jest najlepszym pomys³em, ale mo¿e na"
	echo "razie wystarczyæ."
	echo ""
	read -p "Podaj domy¶ln± nazwê domeny: " domain
	if [ "X${domain}" = "X" ]
	then
		echo ""
		echo "Dobrze, ustawimy 'mydomain.org.pl', ale miej ¶wiadomo¶æ"
		echo "¿e taka domena prawdopodobnie nie istnieje."
		echo ""
		read -p "Naci¶nij Enter" junk
		domain="mydomain.org.pl"
	fi
	echo "[H[J"
	echo "[1m                      Adres Serwera DNS[m"
	echo ""
	echo "Podaj adres w postaci numerycznej serwera DNS. Jest on potrzebny"
	echo "do zamiany nazw (takich jak www.freebsd.org.pl) na adresy IP"
	echo "(takie jak 192.168.1.1). Je¶li nie jest to ustawione poprawnie,"
	echo "bêdziesz musia³ pos³ugiwaæ siê adresami IP podczas ³±czenia siê"
	echo "z innymi maszynami - jest to co najmniej niewygodne."
	echo ""
	echo "Je¶li po prostu naci¶niesz Enter, ustawisz (istniej±cy) serwer"
	echo "o numerze 194.204.159.1 (w sieci TP SA)."
	echo ""
	read -p "Podaj adres IP serwera DNS (w postaci A.B.C.D): " dns
	if [ "X${dns}" = "X" ]
	then
		echo ""
		echo "Dobrze, ustawimy adres DNS serwera na 194.204.159.1, ale"
		echo "niekoniecznie musi to byæ najlepszy serwer w Twojej czê¶ci sieci."
		echo ""
		read -p "Naci¶nij Enter..." junk
		dns="194.204.159.1"
	fi
}
set_phone() {
while [ "X${phone}" = "X" ]
do
	echo "[H[J"
	echo "[1m                        Numer Telefoniczny[m"
	echo ""
	echo "Podaj numer telefoniczny, którego normalnie u¿ywasz, ¿eby"
	echo "dodzwoniæ siê do swojego providera. Powiniene¶ podaæ pe³ny"
	echo "numer, z ewentualnymi przedrostkami, np: 022113355"
	echo ""
	read -p "Podaj numer telefoniczny: " phone
done
}

set_port() {
while [ "X${dev}" = "X" ]
do
	echo "[H[J"
	echo "[1m                        Numer Portu Modemowego[m"
	echo ""
	echo "Podaj numer portu szeregowego, do którego pod³±czony jest modem."
	echo "UWAGA: DOSowy port COM1 to port 0 (cuaa0) we FreeBSD, COM2 -"
	echo "port 1, itd. Podaj tutaj tylko numer, a nie pe³n± nazwê urz±dzenia."
	echo ""
	read -p "Podaj numer portu szeregowego (0,1,2): " dev
done
}

set_speed() {
while [ "X${speed}" = "X" ]
do
	echo "[H[J"
	echo "[1m                      Prêdko¶æ Linii Szeregowej[m"
	echo ""
	echo "Wybierz prêdko¶æ linii szeregowej, której u¿ywa modem."
	echo ""
	echo "UWAGA: Prêdko¶æ linii szeregowej NIE jest tym samym, co prêdko¶æ"
	echo "modemu. Je¶li Twój modem obs³uguje protokó³ V.42 lub MNP"
	echo "(zazwyczaj tak w³a¶nie jest), prêdko¶æ linii szeregowej musi byæ"
	echo "du¿o wiêksza od prêdko¶ci modemu. Np. dla modemów 14.4 kbps z"
	echo "kompresj± nale¿y wybraæ prêdko¶æ 38400 bps, a dla modemów"
	echo "28.8 kbps z kompresj± nale¿y wybraæ prêdko¶æ 115200 bps."
	echo ""
	echo "	1.	9600   bps"
	echo "	2.	14400  bps"
	echo "	3.	28800  bps"
	echo "	4.	38400  bps (modem 14.4 kbps z kompresj±)"
	echo "	5.	57600  bps"
	echo "	6.	115200 bps (modem 28.8 kbps z kompresj±)"
	echo ""
	read -p "Wybierz prêdko¶æ linii szeregowej (1-6): " ans
	case ${ans} in
	1)
		speed=9600
		;;
	2)
		speed=14400
		;;
	3)
		speed=28800
		;;
	4)
		speed=38400
		;;
	5)
		speed=57600
		;;
	6)
		speed=115200
		;;
	*)
		read -p "Z³a warto¶æ! Naci¶nij Enter..." junk
		unset speed
		;;
	esac
done
}

set_timeout() {
while [ "X${timo}" = "X" ]
do
	echo "[H[J"
	echo "[1m                        Czas roz³±czenia[m"
	echo ""
	echo "Podaj czas (w sekundach), po którym, je¶li nie ma ruchu na ³±czu,"
	echo "nast±pi automatyczne roz³±czenie. To pomaga w oszczêdzaniu :-)"
	echo ""
	read -p "Podaj czas roz³±czenia: " timo
done
}

set_user() {
while [ "X${user}" = "X" ]
do
	echo "[H[J"
	echo "[1m                        Nazwa U¿ytkownika[m"
	echo ""
	echo "Podaj nazwê u¿ytkownika (login name), której normalnie u¿ywasz"
	echo "do zalogowania siê do serwera komunikacyjnego providera."
	echo ""
	read -p "Podaj nazwê u¿ytkownika: " user
done
}

set_pass() {
while [ "X${pass}" = "X" ]
do
	echo "[H[J"
	echo "[1m                        Has³o[m"
	echo ""
	echo "Podaj has³o, którego u¿ywasz do zalogowania siê do providera."
	echo ""
	echo "[31mUWAGA: Has³o to zostanie zapisane w czytelnej postaci na"
	echo "dyskietce!!! Je¶li tego nie chcesz... bêdziesz musia³ logowaæ siê"
	echo "rêcznie, tak jak dotychczas. W tym przypadku przerwij ten skrypt"
	echo "przez Ctrl-C.[37m"
	echo ""
	stty -echo
	read -p "Podaj swoje has³o: " pass
	echo ""
	read -p "Podaj powtórnie swoje has³o: " pass1
	stty echo
	echo ""
	if [ "X${pass}" != "X${pass1}" ]
	then
		echo "Has³a nie pasuj± do siebie. Naci¶nij Enter..."
		pass=""
		read junk
		set_pass
	fi
done
}

set_chat() {
echo "[H[J"
while [ "X${chat}" = "X" ]
do
	echo "[1m               Rodzaj dialogu podczas logowania siê[m"
	echo ""
	echo "Jak normalnie przebiega proces logowania siê do serwera"
	echo "komunikacyjnego?"
	echo ""
	echo "1)	[32m......login:[37m ${user}"
	echo "	[32m...password:[37m ********"
	echo "		[36m(tutaj startuje PPP)[37m"
	echo ""
	echo "2)	[32m...username:[37m ${user}			(TP S.A.)"
	echo "	[32m...password:[37m ********"
	echo "		[36m(tutaj startuje PPP)[37m"
	echo ""
	echo "3)	[32m......username:[37m ${user}			(NASK)"
	echo "	[32m......password:[37m ********"
	echo "	[32mportX/..xxx...:[37m ppp"
	echo "		[36m(tutaj startuje PPP)[37m"
	echo ""
	echo "4)	[32mZastosuj CHAP[37m"
	echo ""
	echo "5)	[32mZastosuj PAP[37m"
	echo ""
	read -p "Wybierz 1,2,3,4 lub 5: " chat
	case ${chat} in
	1)
		chat1="TIMEOUT 10 ogin:--ogin: ${user} word: \\\\P"
		chat2="login/password"
		;;
	2)
		chat1="TIMEOUT 10 ername:--ername: ${user} word: \\\\P"
		chat2="TP SA - username/password"
		;;
	3)
		chat1="TIMEOUT 10 ername:--ername: ${user} word: \\\\P port ppp"
		chat2="NASK - username/password/port"
		;;
	4)	chat1="-"
		chat2="CHAP"
		;;
	5)	chat1="-"
		chat2="PAP"
		;;
	*)	echo "Z³a warto¶æ! Musisz wybraæ 1,2 lub 3."
		echo ""
		unset chat
		unset chat2
		;;
	esac
done
}


# Main entry of the script

echo "[H[J"
echo "[1m              Witamy w Automatycznym Konfiguratorze PPP! :-)[m"
echo ""
echo "    PPP jest ju¿ wstêpnie skonfigurowane, tak ¿e mo¿na rêcznie wybieraæ"
echo "numer i rêcznie logowaæ siê do serwera komunikacyjnego. Jest to jednak"
echo "dosyæ uci±¿liwy sposób na d³u¿sz± metê."
echo ""
echo "Ten skrypt postara siê stworzyæ tak± konfiguracjê PPP, ¿eby umo¿liwiæ"
echo "automatyczne wybieranie numeru i logowanie siê, a ponadto pozwoli na"
echo "uruchamianie ppp w tle - nie zajmuje ono wówczas konsoli."
echo ""
echo "Je¶li chcesz kontynuowaæ, naci¶nij [1mEnter[m, je¶li nie - [1mCtrl-C[m."
echo ""
read junk
# Step through the options
set_phone
set_port
set_speed
set_timeout
set_user
set_pass
set_chat
set_resolv

ans="loop_it"
while [ "X${ans}" != "X" ]
do

echo "[H[J"
echo "[1m     Ustawione zosta³y nastêpuj±ce parametry:[m"
echo ""
echo "	1.	Numer telef.:	${phone}"
echo "	2.	Numer portu:	cuaa${dev}"
echo "	3.	Prêdko¶æ portu:	${speed}"
echo "	4.	Czas roz³±cz.:	${timo} s"
echo "	5.	U¿ytkownik:	${user}"
echo "	6.	Has³o:		${pass}"
echo "	7.	Typ dialogu:	${chat} (${chat2})"
echo "	8.	Nazwa domeny:	${domain}"
echo "		Serwer DNS:	${dns}"
echo ""
echo "Je¶li te warto¶ci s± poprawne, po prostu naci¶nij [1mEnter[m"
read -p "Je¶li nie, podaj numer opcji, któr± chcesz zmieniæ (1-8): " ans

a="X${ans}"
case ${a} in
X1)
	unset phone
	set_phone
	;;
X2)
	unset dev
	set_port
	;;
X3)
	unset speed
	set_speed
	;;
X4)
	unset timo
	set_timeout
	;;
X5)
	unset user
	set_user
	;;
X6)
	unset pass
	set_pass
	;;
X7)
	unset chat
	unset chat1
	unset chat2
	set_chat
	;;
X8)
	unset domain
	unset dns
	set_resolv
	;;
X)
	;;
*)
	read -p "Z³y numer opcji! Naci¶nij Enter..." junk
	ans="wrong"
	;;
esac
done

echo ""
echo -n "Generowanie /etc/ppp/ppp.conf file..."
rm -f /etc/ppp/ppp.conf
cp /etc/ppp/ppp.conf.template /etc/ppp/ppp.conf
echo "" >>/etc/ppp/ppp.conf
echo "# This part was generated with $0" >>/etc/ppp/ppp.conf
echo "dialup:" >>/etc/ppp/ppp.conf
echo " set line /dev/cuaa${dev}" >>/etc/ppp/ppp.conf
echo " set phone ${phone}" >>/etc/ppp/ppp.conf
echo " set authkey ${pass}" >>/etc/ppp/ppp.conf
echo " set timeout ${timo}" >>/etc/ppp/ppp.conf
if [ "X${chat1}" = "-" ]
then
	echo "set authname ${user}" >>/etc/ppp/ppp.conf
else
	echo " set login \"${chat1}\"" >>/etc/ppp/ppp.conf
fi
echo " set ifaddr 10.0.0.1/0 10.0.0.2/0 255.255.255.0 0.0.0.0" >>/etc/ppp/ppp.conf

echo " Zrobione."

echo -n "Generowanie /etc/resolv.conf..."
echo "# This file was generated with $0">/etc/resolv.conf
echo "domain ${domain}" >>/etc/resolv.conf
echo "nameserver ${dns}">>/etc/resolv.conf
echo "hostname=\"pico.${domain}\"">>/etc/rc.conf
echo " Zrobione."

echo ""
echo "Ok. Sprawd¼ zawarto¶æ /etc/ppp/ppp.conf, i popraw go je¶li to konieczne."
echo "Nastêpnie mo¿esz wystartowaæ ppp w tle:"
echo ""
echo "	[1mppp -background dialup[m"
echo ""
echo "PAMIÊTAJ, ¿eby uruchomiæ /stand/update ! Inaczej zmiany nie zostan± zapisane"
echo "na dyskietce!"
echo ""
echo "Ok. Je¶li Twój plik /etc/ppp/ppp.conf jest prawid³owy (co jest dosyæ"
echo -n "prawdopodobne :-), czy chcesz teraz uruchomiæ po³±czenie dialup? (t/n) "
read ans
opts=""
while [ "X${ans}" = "Xt" ]
do
	echo "[H[J"
	if [ "X${opts}" = "X" ]
	then
		echo "Wystartujemy 'ppp' z poni¿szymi opcjami:"
		echo ""
		echo "		ppp -background dialup"
		echo ""
		echo -n "Czy chcesz je zmienic?? (t/n) "
		read oo
		if [ "X${oo}" = "Xt" ]
		then
			read -p "Podaj opcje ppp: " opts
		else
			opts="-background dialup"
		fi
		echo ""
		echo ""
	fi
	echo "Uruchamiam po³±czenie dialup. Proszê czekaæ dopóki nie pojawi siê"
	echo "komunikat 'PPP Enabled'..."
	echo ""
	ppp -background dialup
	if [ "X$?" != "X0" ]
	then
		echo -n "Po³±czenie nie powiod³o siê. Spróbowaæ jeszcze raz?  (t/n) "
		read ans
		if [ "X${ans}" != "Xt" ]
		then
			echo "Spróbuj pó¼niej. Sprawd¼ równie¿ plik konfiguracyjny /etc/ppp/ppp.conf."
			echo ""
		fi
	else
		echo ""
		echo "Gratulujê! Jeste¶ on-line."
		echo ""
		exit 0
	fi
done
