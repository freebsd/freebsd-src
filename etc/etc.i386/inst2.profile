stty status '^T'

# Set shell to ignore SIGINT (2), but not children;
# shell catches SIGQUIT (3) and returns to single user after fsck.
trap : 2
trap : 3	# shouldn't be needed

HOME=/; export HOME
PATH=/sbin:/bin:/usr/sbin:/usr/bin; export PATH

if [ -e /fastboot ]
then
	echo Fast boot ... skipping disk checks
else
	echo Automatic reboot in progress...
	fsck -p
	case $? in
	0)
		;;
	2)
		exit 1
		;;
	4)
		echo "fsck changed the root file system..."
		echo -n "halting, press return to reboot after the system has "
		echo "halted"
		halt
		echo "reboot failed... help!"
		exit 1
		;;
	8)
		echo "Automatic file system check failed... help!"
		exit 1
		;;
	12)
		echo "Reboot interrupted"
		exit 1
		;;
	130)
		# interrupt before catcher installed
		exit 1
		;;
	*)
		echo "Unknown error in reboot"
		exit 1
		;;
	esac
fi

trap "echo 'Reboot interrupted'; exit 1" 3

swapon -a

umount -a >/dev/null 2>&1
mount -a -t nonfs
rm -f /fastboot		# XXX (root now writeable)

# clean up left-over files
rm -f /etc/nologin
rm -f /var/spool/uucp/LCK.*
rm -f /var/spool/uucp/STST/*
(cd /var/run && { rm -rf -- *; cp /dev/null utmp; chmod 644 utmp; })

echo clearing /tmp

echo -n standard daemons:
echo -n ' update';		update
echo '.'
echo ''

TERM=pc3	# terminal emulator, for elvis
TERMCAP="\
pc3|ibmpc3:li#25:co#80:am:bs:bw:eo:cd=\E[J:ce=\E[K:cl=\Ec:cm=\E[%i%2;%2H:\
do=\E[B:ho=\E[;H:nd=\E[C:up=\E[A:so=\E[7m:se=\E[0m:us=\E[4m:ue=\E[0m:\
:ac=l\332q\304k\277x\263j\331m\300w\302u\264v\301t\303n\305:\
:kb=^h:kh=\E[Y:ku=\E[A:kd=\E[B:kl=\E[D:kr=\E[C:"
OPSYSTEM=FreeBSD
RELEASE="1.0 GAMMA"
export TERMCAP
export TERM
echo "${OPSYSTEM} Base System Release ${RELEASE}"
echo ""
echo "Congratulations, you've got ${OPSYSTEM} on the hard disk!"
echo ""
echo "Press the return key for more installation instructions"
read junkit
echo ""
echo "To finish installation:"
echo "Pick a temporary directory by running set_tmp_dir.  Make sure it's"
echo "in a place with lots of space, probably under /usr."
echo "Then, load the remaining distribution files into that temporary"
echo "directory by issuing one of the following commands:"
echo ""
echo "       load_fd	load_qic_tape	load_scsi_tape"
echo ""
echo "or by fetching the files with ftp (see the installation notes for"
echo "information on how to do that)."
echo ""
echo "Once this is complete, extract the distribution files by issuing the"
echo "command 'extract <distribution>'  where <distribution> is the base name"
echo "of the distribution files, e.g. 'base10'."
echo ""
echo "Once all of the filesets you wish to install have been extracted,"
echo "enter the command 'configure' to finish setting up the system"
echo " "
echo "If you should wish to uninstall ${OPSYSTEM}, delete the partition by using the"
echo "DOS 5 FDISK program. If installed on the entire drive, use the FDISK/MBR"
echo "to remove the ${OPSYSTEM} bootstrap from the drive."
echo 'erase ^?, werase ^H, kill ^U, intr ^C'
stty newcrt werase  intr  kill  erase  9600
umask 0
set_tmp_dir()
{
	def_tmp_dir=`pwd`
	if [ "$def_tmp_dir" = "/" ]; then
		def_tmp_dir=/usr/distrib
	fi
	echo -n "what dir should be used for temporary files? [$def_tmp_dir] "
	read tmp_dir
	if [ "$tmp_dir" = "" ]; then
		tmp_dir=$def_tmp_dir
	fi
	if [ ! -d "$tmp_dir" ]; then
		/bin/rm -rf $tmp_dir
		mkdir -p $tmp_dir
	fi
}
tmp_dir()
{
	if [ "$tmp_dir" = "" ]; then
		set_tmp_dir
	fi
	cd $tmp_dir
}
load_fd()
{
	tmp_dir
	which=
	while [ "$which" != "a" -a "$which" != "b" ]; do
		echo -n "read from which floppy drive?  [a or b] "
		read which
	done
	while echo -n "Insert floppy (hit ^C to terminate, enter to load): "
	do
		read foo
		mread "$which:*.*" .
	done
}
load_qic_tape()
{
	tmp_dir
	echo -n "Insert tape into QIC tape drive and hit return to continue: "
	read foo
	tar xvf /dev/rwt0
}
load_scsi_tape()
{
	tmp_dir
	echo -n "Insert tape into SCSI tape drive and hit return to continue: "
	read foo
	tar xvf /dev/nrst0
}
extract()
{
	tmp_dir
	echo -n "Would you like to be verbose about this? [n] "
	read verbose
	case $verbose in
		y*|Y*)
			tarverbose=--verbose
			;;
		*)
			tarverbose=
			;;
	esac
	#XXX ugly hack to eliminate busy files, copy them to /tmp and use them
	#from there...
	cp -p /bin/cat /usr/bin/gunzip /usr/bin/tar /tmp
	/tmp/cat "$@"* | /tmp/gunzip | (cd / ; /tmp/tar --extract --file - --preserve-permissions ${tarverbose} )
	rm -f /bin/cat /tmp/gunzip /tmp/tar
	sync
}
configure()
{
	echo    "You will now be prompted for information about this"
	echo	"machine.  If you hit return, the default answer (in"
	echo	"brackets) will be used."

	echo    ""
	echo -n "What is this machine's hostname? [unknown.host.domain] "
	read hname

	if [ "$hname" = "" ]; then
		hname=unknown.host.domain
	fi
	echo $hname > /etc/myname
	proto_domain=`echo $hname | sed -e 's/[^.]*\.//'`
	
	echo    ""
	echo    "What domain is this machine in (this is NOT its YP"
	echo -n "domain name)? [$proto_domain] "
	read dname

	if [ "$dname" = "" ]; then
		dname=$proto_domain
	fi

	echo	""
	echo -n "Does this machine have an ethernet interface? [y] "
	read resp
	case "$resp" in
	        n*)
                ;;
        *)
		intf=
		while [ "$intf" = "" ]; do
			echo -n "What is the primary interface name (i.e. we0, etc.)? "
			read intf
		done
		echo -n "What is the hostname for this interface? [$hname] "
		read ifname
		if [ "$ifname" = "" ]; then
			ifname=$hname
		fi
		ifaddr=
		while [ "$ifaddr" = "" ]; do
			echo -n "What is the IP address associated with this interface? "
			read ifaddr
		done
		echo "$ifaddr    $ifname `echo $ifname | sed -e s/\.$dname//`" \
			>> /etc/hosts

		echo -n "Does this interface have a special netmask? [n] "
		read resp
		case "$resp" in
		        y*)
				echo -n "What is the netmask? [0xffffff00] "
				read ifnetmask
				if [ "$ifnetmask" = "" ]; then
					ifnetmask=0xffffff00
				fi
	       		        ;;
			*)
				ifnetmask=
				;;
		esac
		
		echo -n "Does this interface need additional flags? [n] "
		read resp
		case "$resp" in
			y*)
				echo -n "What flags? [llc0] "
				read ifflags
				if [ "$ifflags" = "" ]; then
					ifflags=llc0
				fi
				;;
			*)
				ifflags=
				;;
		esac
		
		echo "inet $ifname $ifnetmask $ifflags" > /etc/hostname.$intf

		echo    ""
		echo    "WARNING: if you have any more ethernet interfaces, you"
		echo	"will have to configure them by hand.  Read the comments"
		echo	"in /etc/netstart to learn how to do this"
                ;;
	esac

	sync

	echo	""
	echo	"OK.  You should be completely set up now."
	echo	"You should now reboot your machine by issuing the 'reboot' command"
	echo	"after removing anything that happens to be in your floppy drive."
}
