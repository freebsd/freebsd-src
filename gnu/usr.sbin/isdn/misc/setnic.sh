# First Parameter one out of the following:
#	5000	Niccy 5000
#	3008	Niccy 3008
#	3009	Niccy 3009
#	1000	Niccy 1000
#	tel* , TEL*  TELES S0
#
# Second Parameter is optional:
#	E* e*	Euro ISDN EDSS1
#	1T* 1t* t* T* 1TR6 (old german protocol) (default for the moment)

PATH=/sbin:/bin/:/usr/bin:/usr/sbin
SYSTEM=`uname`
VER=`uname -a | cut -d' ' -f3`
case $SYSTEM in
NetBSD)
	SN=netbsd
	;;
FreeBSD)
        case $VER in
	1.0*|1.1*)
		SN=386bsd
		;;
	2.0*)
		SN=kernel
		;;
        *)
                echo System $SYSTEM Version $VER not supported
                exit
        esac
        ;;
*)
	echo System $SYSTEM not supported
	exit
esac
	
if [ "$2" = "" ]
then
	LIB=tr6
else
	case $2 in
	E*|e*)
		LIB=eds
		;;
	1t*|1T*|t*|T*)
		LIB=tr6
		;;
	*)
		echo library $2 not supported
		exit
	esac
fi


case $1 in

5000)
	rm -f /$SN /isdn/lib/all.nic
	ln /$SN.5000 /$SN
	ln /isdn/lib/all.$LIB.5000 /isdn/lib/all.nic
	/isdn/bin/mkdev 5000
	;;
3008)
	rm -f /$SN /isdn/lib/all.nic
	ln /$SN.3008 /$SN
	ln /isdn/lib/all.$LIB.3008 /isdn/lib/all.nic
	/isdn/bin/mkdev 3008
	;;
3009)
	rm -f /$SN /isdn/lib/all.nic
	ln /$SN.3009 /$SN
	ln /isdn/lib/all.$LIB.3009 /isdn/lib/all.nic
	/isdn/bin/mkdev 3009
	;;
1000|tel*|TEL*)
	rm -f /$SN /isdn/lib/all.nic
	ln /$SN.1000 /$SN
	/isdn/bin/mkdev 1000
	;;
esac

ls -l /$SN*

echo please reboot the system by typing: fastboot
