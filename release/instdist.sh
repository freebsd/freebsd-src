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
# $Id: instdist.sh,v 1.17 1994/11/20 15:18:56 jkh Exp $

if [ "$_INSTINST_SH_LOADED_" = "yes" ]; then
	return 0
else
	_INSTINST_SH_LOADED_=yes
fi

# Grab the miscellaneous functions.
. /stand/miscfuncs.sh

# Set some reasonable defaults.
TAR=tar
TAR_FLAGS="--unlink -xvf"
MNT=/mnt

# Set the initial state for media installation.
media_set_defaults()
{
	media_type=""
	media_device=""
	media_distribution=""
	distrib_subdir=""
	clear="--clear"
	ipaddr=""
	hostname=""
	ether_intr=""
	domain=""
	netmask="0xffffff00"
	ifconfig_flags=""
	remote_hostip=""
	tmp_dir="/usr/tmp"
	ftp_path=""
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
storage."; then return 1; fi
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
"FreeBSD is distributed from a number of sites on the Internet.\n\
Please select the site closest to you or \"other\" if you'd like\n\
to specify another choice.  Also note that not all sites carry\n\
every possible distribution!  Distributions other than the basic\n\
binary set are only guaranteed to be available from the Primary site." \
-1 -1 10 \
   "Primary" "ftp://ftp.freebsd.org/pub/FreeBSD/${DISTNAME}" \
   "U.S-2" "ftp://ftp.dataplex.net/pub/FreeBSD/${DISTNAME}" \
   "U.S-3" "ftp://kryten.atinc.com/pub/FreeBSD/${DISTNAME}" \
   "U.S-4" "ftp://ref.tfs.com/pub/FreeBSD/${DISTNAME}" \
   "Taiwan" "ftp://netbsd.csie.nctu.edu.tw/pub/FreeBSD/${DISTNAME}" \
   "Australia" "ftp://ftp.physics.usyd.edu.au/FreeBSD/${DISTNAME}" \
   "France" "ftp://ftp.ibp.fr/pub/freeBSD/${DISTNAME}" \
   "Finland" "ftp://nic.funet.fi:/pub/unix/FreeBSD/${DISTNAME}" \
   "Russia" "ftp://ftp.kiae.su/FreeBSD/${DISTNAME}" \
   "other" "None of the above.  I want to specify my own." \
      2> ${TMP}/menu.tmp.$$
	retval=$?
	answer=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval $retval; then return 1; fi
	case $answer in
	Primary)
		ftp_path="ftp://ftp.freebsd.org/pub/FreeBSD/${DISTNAME}"
	;;

	U.S-2)
		ftp_path="ftp://ftp.dataplex.net/pub/FreeBSD/${DISTNAME}"
	;;

	U.S-3)
		ftp_path="ftp://kryten.atinc.com/pub/FreeBSD/${DISTNAME}"
	;;

   	U.S-4)
		ftp_path="ftp://ref.tfs.com/pub/FreeBSD/${DISTNAME}"
	;;

	Taiwan)
		ftp_path="ftp://netbsd.csie.nctu.edu.tw/pub/FreeBSD/${DISTNAME}"
	;;

	Australia)
		ftp_path="ftp://ftp.physics.usyd.edu.au/FreeBSD/${DISTNAME}"
	;;

	France)
		ftp_path="ftp://ftp.ibp.fr/pub/freeBSD/${DISTNAME}"
	;;

	Finland)
		ftp_path="ftp://nic.funet.fi:/pub/unix/FreeBSD/${DISTNAME}"
	;;

	Russia)
		ftp_path="ftp://ftp.kiae.su/FreeBSD/${DISTNAME}"
	;;

	other)
		title="FTP Installation Information"
		default_value="$ftp_path"
		if ! input \
"Please specify the machine and directory location of the
distribution you wish to load.  This should be either a \"URL style\"
specification (e.g. ftp://ftp.freeBSD.org/pub/FreeBSD/...) or simply
the name of a host to connect to.  If only a host name is specified,
the installation assumes that you will properly connect and \"mget\"
the files yourself."; then return 1; fi
		ftp_path=$answer
	;;
	esac
}

media_extract_dist()
{
	if [ -f do_cksum.sh ]; then
		message "Verifying checksums for distribution.  Please wait!"
		if sh ./do_cksum.sh; then
			if [ -f extract.sh ]; then
				message "Extracting distribution.  Please wait!"
				sh ./extract.sh < /dev/ttyv1 > /dev/ttyv1 2>&1
				dialog $clear --title "Extraction Complete" --msgbox "Please press return to continue" -1 -1
			else
				error "No installation script found!"
			fi
		else
			error "Checksum error(s) found.  Please check media!"
		fi
	else    
		error "Improper distribution.  No checksum script found!"
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
		umount ${MNT}
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
			  $clear --yesno "Please enter the next diskette and press OK to continue or Cancel if finished" -1 -1; then
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
		if ! echo $media_device | grep -q -v 'ftp://'; then
			message "Fetching distribution using ncftp.\nUse ALT-F2 to see output, ALT-F1 to return."
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
of installation.  With repeated passes through this screen,\n\
you'll be given the chance to load one or all of them.  Mandatory \n\
distributions MUST be loaded!  Please also note that the secrdist\n\
is NOT FOR EXPORT from the U.S.  Please don't endanger U.S. ftp\n\
sites by getting it illegally, thanks!  When finished, select Cancel" -1 -1 10 \
  "?diskfree"  "How much disk space do I have free?" \
  "bindist" "Binary base files (mandatory - $BINSIZE)" \
  "games" "Games and other frivolities (optional - $GAMESIZE)" \
  "manpages" "Manual pages (optional - $MANSIZE)" \
  "proflibs" "Profiled libraries (optional - $PROFSIZE)" \
  "dict" "Dictionary files for spelling checkers (optional - $DICTSIZE)" \
  "srcdist" "Full sources for everything but DES (optional - $SRCSIZE)" \
  "secrdist" "DES encryption code (and sources) (optional - $SECRSIZE)" \
  "compat1xdist" "FreeBSD 1.x binary compatability (optional - $COMPATSIZE)" \
  "packages" "Optional binary software distributions (user choice)" \
     2> ${TMP}/menu.tmp.$$
	retval=$?
	media_distribution=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval $retval; then return 1; fi
	if [ "$media_distribution" = "?diskfree" ]; then
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
	default_value="$distrib_subdir"
	title="Distribution Subdirectory"
	if input \
"If the distributions are in a subdirectory of the mount point,
please enter it here (no leading slash - it should be relative
to the mount point).  The directory you enter should be the
*parent* directory of any distribution subdirectories."; then
		if [ "$answer" != "" ]; then
			media_device=${media_device}/$answer
			distrib_subdir=$answer
		fi
	fi
}

# Get values into $media_type and $media_device.  Call network initialization
# if necessary.
media_chose()
{
	while [ "$media_device" = "" ]; do

	dialog $clear --title "Installation From" \
--menu \
"Before installing a distribution, you need to chose and/or configure\n\
a method of installation.  Please pick from one of the following options.\n\
If none of the listed options works for you, then your best bet may be to\n\
simply hit ESC twice to get a subshell and proceed manually on your own.\n\
If you are already finished with the installation process, select cancel\n\
to proceed." -1 -1 7 \
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
system?  FreeBSD supports the following types:\n" -1 -1 3 \
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
system?  FreeBSD supports the following types:\n" -1 -1 2 \
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
drive it's /dev/fd1\n"; then
			media_device=$answer
			if echo $media_device | grep -q -v 'fd://'; then
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
for this to work!\n"; then continue; fi
		default_value=""
		if input \
"Do you wish to specify any options to NFS?  If you're installing
from a Sun 4.1.x system, you may wish to specify resvport to allow
installation over a priviledged port.  When using a slow ethernet
card or network link, rsize=4096,wsize=4096 may also prove helpful.
Options, if any, should be separated by commas."; then
			if [ "$answer" != "" ]; then
				nfs_options="-o $answer"
			fi
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
of the distribution when we come back." -1 -1
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
