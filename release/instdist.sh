#!/stand/sh
#
# instdist - Install a distribution from some sort of media.
#
# Written:  November 11th, 1994
# Copyright (C) 1994 by Jordan K. Hubbard
#
# Permission to copy or use this software for any purpose is granted
# provided that this message stay intact, and at this location (e.g. no
# putting your name on top after doing something trivial like reindenting
# it, just to make it look like you wrote it!).
#
# $Id: instdist.sh,v 1.2 1994/11/17 11:53:13 jkh Exp $

if [ "$_INSTINST_SH_LOADED_" = "yes" ]; then
	return 0
else
	_INSTINST_SH_LOADED_=yes
fi

# Grab the miscellaneous functions.
. miscfuncs.sh

# Set some reasonable defaults.
TAR=tar
TAR_FLAGS="--unlink -xvf"
MNT=/mnt

# Set the initial state for media installation.
media_set_defaults() {
	media_type=""
	media_device=""
	media_distribution=""
	clear="--clear"
	ipaddr=""
	hostname=""
	ether_intr=""
	domain=""
	netmask="0xffffff00"
	ifconfig_flags=""
	remote_hostip=""
	tmp_dir="/usr/tmp"
	ftp_path="ftp://ftp.freebsd.org/pub/FreeBSD/2.0-ALPHA"
	nfs_path=""
	nfs_options=""
	serial_interface="/dev/tty00"
	serial_speed="38400"
}

# Set the installation media to undefined.
media_reset()
{
	media_device=""
	media_type=""
	media_distribution=""
}

# Set the location of our temporary unpacking directory.
media_set_tmpdir()
{
	title="Chose temporary directory"
	default_value="/usr/tmp"
	if ! input \
"Please specify the name of a directory containing enough free
space to hold the temporary files for this distribution.  At
minimum, a binary distribution will require around 21MB of
temporary space.  At maximum, a srcdist may take 30MB or more.
If the directory you specify does not exist, it will be created
for you.  If you do not have enough free space to hold both the
packed and unpacked distribution files, consider using the NFS
or CDROM installation methods as they require no temporary
storage.\n\n"; then return 1; fi
	tmp_dir=$answer
	mkdir -p $tmp_dir
	return 0
}

media_cd_tmpdir()
{
	if ! cd $tmp_dir; then
		error "No such file or directory for ${tmp_dir}, sorry!  Please fix this and try again."
		return 1
	fi
}

media_rm_tmpdir()
{
	cd /
	if dialog --title "Delete contents?" $clear --yesno \
          "Do you wish to delete the contents of ${tmp_dir}?" -1 -1; then
		rm -rf $tmp_dir/*
	fi
}

media_select_ftp_site()
{
	dialog $clear --title "Please specify an ftp site" \
	--menu \
"FreeBSD is distributed from a number of sites on the Internet \n\
in order to more evenly distribute network load and increase \n\
its availability users who might be far from the main ftp sites \n\
or unable to get a connection.  Please select the site closest \n\
to you or select \"other\" if you'd like to specify your own \n\
choice.\n\n" 20 76 7 \
"Please select one of the following:" 20 76 6 \
   "ftp://ftp.freebsd.org/pub/FreeBSD/${DISTNAME}" "Primary U.S. ftp site" \
   "ftp://ftp.dataplex.net/pub/FreeBSD/${DISTNAME}" "United States" \
   "ftp://netbsd.csie.nctu.edu.tw/pub/FreeBSD/${DISTNAME}" "Taiwan" \
   "ftp://ftp.physics.usyd.edu.au/FreeBSD/${DISTNAME}" "Australia" \
   "ftp://ftp.ibp.fr/pub/freeBSD/${DISTNAME}" "France" \
   "ftp://nic.funet.fi:/pub/unix/FreeBSD/${DISTNAME}" "Finland" \
   "other" "None of the above.  I want to specify my own." \
      2> ${TMP}/menu.tmp.$$
	retval=$?
	answer=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval $retval; then return 1; fi
	if [ "$answer" = "other" ]; then
		title="FTP Installation Information"
		default_value="$ftp_path"
		if ! input \
"Please specify the machine and directory location of the
distribution you wish to load.  This should be either a \"URL style\"
specification (e.g. ftp://ftp.freeBSD.org/pub/FreeBSD/...) or simply
the name of a host to connect to.  If only a host name is specified,
the installation assumes that you will properly connect and \"mget\"
the files yourself.\n\n"; then return 1; fi
	fi
	ftp_path=$answer
}

media_extract_dist()
{
	if [ -f extract.sh ]; then
		message "Extracting distribution..  Please wait!"
		sh ./extract.sh < /dev/ttyv1 > /dev/ttyv1 2>&1
	else    
		error "Improper distribution.  No installation script found!"
	fi
}

media_install_set()
{
	case $media_type in
	cdrom|nfs|ufs|doshd)
		message "Extracting ${media_distribution} using ${media_type}."
		cd ${media_device}/${media_distribution}
		media_extract_dist
		cd /
		return
	;;

	tape)
		if ! media_set_tmpdir; then return; fi
		if ! media_cd_tmpdir; then return; fi
		confirm "Please mount tape for ${media_device}."
		if [ "$media_device" = "ftape" ]; then
			dialog --title "Results of tape extract" $clear \
			  --prgbox "ft | $TAR $TAR_FLAGS -" 10 72
		else
			dialog --title "Results of tape extract" $clear \
			  --prgbox "$TAR $TAR_FLAGS $media_device" 10 72
		fi
		media_extract_dist
		media_rm_tmpdir
	;;

	dosfd)
		if ! media_set_tmpdir; then return; fi
		if ! media_cd_tmpdir; then return; fi
		copying="yes"
		while [ "$copying" = "yes" ]; do
			if dialog --title "Insert distribution diskette" \
			  $clear --yesno "Please enter the next diskette and press OK to continue or Cancel if finished" 5 72; then
				if ! mount_msdos ${media_device} ${MNT}; then
					error "Unable to mount floppy!  Please correct."
				else
					( tar -cf - -C ${MNT} . | tar -xvf - ) >/dev/ttyv1 2>&1
					umount ${MNT}
				fi
			else
				copying="no"
			fi
		done
		media_extract_dist
		media_rm_tmpdir
		return
	;;

	ftp)
		if ! media_set_tmpdir; then return; fi
		if ! media_cd_tmpdir; then return; fi
		if ! echo $media_device | grep -v 'ftp://'; then
			message "Fetching distribution using ncftp.  Use ALT-F2 to see output, ALT-F1 to return."
			if ! ncftp $media_device/${media_distribution}/* < /dev/null > /dev/ttyv1 2>&1; then
				error "Couldn't fetch ${media_distribution} distribution from ${media_device}!"
			else
				media_extract_dist
			fi
		else
			dialog --clear
			ftp $media_device
			dialog --clear
			media_extract_dist
		fi
		media_rm_tmpdir
		return
	;;
	esac
}

media_select_distribution()
{
	media_distribution=""
	while [ "$media_distribution" = "" ]; do

	dialog $clear --title "Please specify a distribution to load" \
	--menu \
"FreeBSD is separated into a number of distributions for ease \n\
of installation.  Depending on how much hard disk space you have \n\
available, you may chose to load one or all of them.  Optional \n\
and mandatory distributions are so noted.  Please also note that \n\
the secrdist is NOT FOR EXPORT from the U.S.!  Please don't \n\
endanger U.S. ftp sites by getting it illegally.  Thank you!\n\n" \
"Please select one (we'll come back to this menu later):" 20 76 6 \
  "?diskfree"  "Uh, first, how much disk space do I have free?" \
  "bindist" "The ${DISTNAME} base distribution (mandatory - 80MB)" \
  "srcdist" "The ${DISTNAME} source distribution (optional - 120MB)" \
  "secrdist" "The ${DISTNAME} DES distribution (optional - 5MB)" \
  "compat1xdist" "The FreeBSD 1.x binary compatability dist (optional - 2MB)"\
  "packages" "The ${DISTNAME} optional software distribution (user choice)" \
     2> ${TMP}/menu.tmp.$$
	retval=$?
	media_distribution=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval $retval; then return 1; fi
	if [ $media_distribution = "?diskfree" ]; then
		if df -k > ${TMP}/df.out; then
			dialog $clear \
			--title "How much free space do I have?" \
			--textbox ${TMP}/df.out 15 76
		else
			error "Couldn't get disk usage information! :-("
		fi
		media_distribution=""
	fi
	done
}

media_get_possible_subdir()
{
	default_value=""
	title="Distribution Subdirectory"
	if input \
"If the distributions are in a subdirectory of the mount point,
please enter it here (no leading slash - it should be relative
to the mount point).\n\n"; then
		if [ "$answer" != "" ]; then
			media_device=${media_device}/$answer
		fi
	fi
}

# Get values into $media_type and $media_device.  Call network initialization
# if necessary.
media_chose() {
	while [ "$media_device" = "" ]; do

	dialog $clear --title "Installation From" \
--menu "Before installing a distribution, you need to chose \n\
and/or configure your method of installation.  Please pick from \n\
one of the following options.  If none of the listed options works \n\
for you then your best bet may be to simply hit ESC twice to get \n\
a subshell and proceed manually on your own.  If you are already \n\
finished with installation, select cancel to go on.\n\n\
	Please choose one of the following:" 20 72 7 \
	"?Kern" "Please show me the kernel boot messages again!" \
	"Tape" "Load distribution from SCSI, QIC or floppy tape" \
	"CDROM" "Load distribution from SCSI or Mitsumi CDROM" \
	"DOS" "Load from DOS floppies or a DOS hard disk partition" \
	"FTP" "Load distribution using FTP" \
	"UFS" "Load the distribution from existing UFS partition" \
	"NFS" "Load the distribution over NFS" 2> ${TMP}/menu.tmp.$$
	retval=$?
	choice=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval $retval; then return 1; fi

	case $choice in
	?Kern)
		if dmesg > ${TMP}/dmesg.out; then
			dialog $clear \
			--title "What do I have in this machine again?" \
			--textbox ${TMP}/dmesg.out 22 76
		else
			error "Couldn't get dmesg information! :-("
		fi
	;;

	Tape)
		dialog $clear --title "Chose Tape Type" \
--menu "Which type of tape drive do you have attached to your \n\
system?  FreeBSD supports the following types:\n\n\
		Choose one of the following:" 20 72 3 \
		"SCSI" "SCSI tape drive attached to supported SCSI controller" \
		"QIC" "QIC tape drive (Colorado Jumbo, etc)" \
		"floppy" "Floppy tape drive" \
			2> ${TMP}/menu.tmp.$$
		retval=$?
		choice=`cat ${TMP}/menu.tmp.$$`
		rm -f ${TMP}/menu.tmp.$$
		if ! handle_rval $retval; then continue; fi
		media_type=tape;
		case $choice in
			SCSI)
				media_device=/dev/rst0
			;;
			QIC)
				media_device=/dev/rwt0
			;;
			floppy)
				media_device=ftape
			;;
		esac
	;;

	CDROM)
		dialog $clear --title "Chose CDROM Type" \
--menu "Which type of CDROM drive do you have attached to your \n\
system?  FreeBSD supports the following types:\n\n\
		Choose one of the following:" 15 72 2 \
		"SCSI" "SCSI CDROM drive attached to supported SCSI controller" \
		"Mitsumi" "Mitsumi CDROM drive" \
			2> ${TMP}/menu.tmp.$$
		retval=$?
		choice=`cat ${TMP}/menu.tmp.$$`
		rm -f ${TMP}/menu.tmp.$$
		if ! handle_rval $retval; then continue; fi
		media_type=cdrom;
		case $choice in
			SCSI)
				media_device=/dev/cd0a
			;;
			Mitsumi)
				media_device=/dev/mcd0a
			;;
		esac
		if ! mount_cd9660 $media_device ${MNT} > /dev/ttyv1 2>&1; then
			error "Unable to mount $media_device on ${MNT}"
			media_device=""
		else
			media_device=${MNT}
			media_get_possible_subdir
		fi
	;;

	DOS)
		default_value="/dev/fd0"
		if input \
"Please specify the device pointing at your DOS partition or
floppy media.  For a hard disk, this might be something like
/dev/wd0h or /dev/sd0h (as identified in the disklabel editor).
For the "A" floppy drive, it's /dev/fd0, for the "B" floppy
drive it's /dev/fd1\n\n"; then
			media_device=$answer
			if echo $media_device | grep -v 'fd://'; then
				if ! mount_msdos $media_device ${MNT} > /dev/ttyv1 2>&1; then
					error "Unable to mount $media_device"
					media_device=""
				else
					message "$media_device mounted successfully"
					media_type=doshd
					media_device=${MNT}
					media_get_possible_subdir
				fi
			else
				media_type=dosfd
			fi
		fi
	;;

	FTP)
		if ! network_setup; then continue; fi
		if media_select_ftp_site; then
			media_type=ftp
			media_device=$ftp_path
		fi
	;;

	NFS)
		if ! network_setup; then continue; fi
		title="NFS Installation Information"
		default_value="$nfs_path"
		if ! input \
"Please specify a machine and directory mount point for the
distribution you wish to load.  This must be in machine:dir
format (e.g. zooey:/a/FreeBSD/${DISTNAME}).  The remote
directory *must* be be exported to your machine (or globally)
for this to work!\n\n"; then continue; fi
		default_value=""
		if input \
"Do you wish to specify any options to NFS?  If you're installing
from a Sun 4.1.x system, you may wish to specify resvport to allow
installation over a priviledged port.  When using a slow ethernet
card or network link, rsize=4096,wsize=4096 may also prove helpful.
Options, if any, should be separated by commas."; then
			nfs_options="-o $answer"
		fi
		media_type=nfs
		nfs_path=$answer
		if ! mount_nfs $nfs_options $nfs_path ${MNT} > /dev/ttyv1 2>&1; then
			error "Unable to mount $nfs_path"
		else
			message "$nfs_path mounted successfully"
			media_device=${MNT}
			media_get_possible_subdir
		fi
	;;

	UFS)
		dialog $clear --title "User Intervention Requested" --msgbox "
Please mount the filesystem you wish to use somewhere convenient and
exit the shell when you're through.  I'll ask you for the location
of the distribution when we come back." 12 72
		dialog --clear
		/stand/sh
		title="Please enter directory"
		default_value="${MNT}/bindist"
		if input "Ok, now give me the full pathname of the directory where you've got the distribution."; then
			if [ ! -f $answer/extract.sh ]; then
				error "That's not a valid distribution"
			else
				media_type=ufs
				media_device=$answer
			fi
		fi
	;;
	esac
	done
}
