#! /bin/sh

if [ -f /bin/uname -o -f /usr/bin/uname ]; then
	set `uname -a | tr '[A-Z]' '[a-z]'`
#	set `cat test | tr '[A-Z]' '[a-z]'`
	case "$1" in
		convexos) case "$3" in
			10.*) guess="convexos10" ;;
		    	esac
			;;
		aix) case "$4" in
			3) case "$3" in
				1) guess="aix3.1" ;;
				2) guess="aix3.2" ;;
				esac
				;;
			esac
			;;
		sinix-m)
			guess=sinix-m
			;;
		sunos|solaris)
			case "$3" in
			4.1*) guess="sunos4" ;;
			5.1)   guess="sunos5.1" ;;
			5.*)   guess="sunos5.2" ;;
			esac
			;;
		irix) case "$3" in
			4.*) guess="irix4" ;;
			5.*) guess="irix5" ;;
			esac
			;;
		"a/ux") case "$3" in
			2.*) guess="aux2" ;;
			3.*) guess="aux3" ;;
			esac
			;;
		ultrix)
			guess="ultrix"
			;;
		hp-ux)  case "$3" in
			*.10.*) guess="hpux-adj" ;;
			*.09.03) case "$5" in
				9000/3*) guess="hpux-adj" ;;
				*) guess="hpux" ;;
				esac ;;
			*) guess="hpux" ;;
			esac
			;;
		linux)  guess="linux" ;;

		osf1) 	case "$5" in 
			alpha) guess="decosf1" ;;
			esac
			;;
		"bsd/386")
			guess="bsdi"
			;;
		"freebsd")
			guess="freebsd"
			;;
		"netbsd")
			guess="netbsd"
			;;
		# now the fun starts - there are vendors that
		# do not really identify their OS in uname.
		# Fine - now I look at our version and hope
		# that nobody else had this marvellous idea.
		# I am not willing to mention the vendor explicitly
		*)	# Great ! - We are dealing with an industry standard !
			if [ -f /unix ]; then
				#
				# looks like this thing has the license
				# to call itself Unix
				#
				case "$3" in
					3.2.*)
						case "$4" in
							v*)
								(i386) >/dev/null 2>&1 && guess=ptx;;
						esac
				esac
			fi
			;;
	esac
fi

if [ "0$guess" != "0" ]; then
	echo $guess
    	exit 0
fi

if [ -f /bin/machine ]; then
	echo `/bin/machine`
	exit 0
fi

if [ -f /usr/convex/vers ]; then
	set `/usr/convex/vers /vmunix`
	case "$2" in
	    	9.0) echo "convexos9" 
		     exit 0 ;;
	esac
fi

if [ -d /usr/lib/NextStep ]; then
	echo next
	exit 0 
fi

if [ -f /netbsd ]; then
	echo netbsd
	exit 0
fi

if [ -f /lib/clib -a -f /lib/libc ]; then
	echo domainos
	exit 0
fi

case "$guess" in
	'') guess="none"
esac

echo $guess
