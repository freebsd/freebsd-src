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
# $Id: instdist.sh,v 1.11 1995/02/02 23:09:30 jkh Exp $

if [ "${_INSTINST_SH_LOADED_}" = "yes" ]; then
	return 0
else
	_INSTINST_SH_LOADED_=yes
fi

# Grab the miscellaneous functions.
. /stand/scripts/miscfuncs.sh

# Set the initial state for media installation.
media_set_defaults()
{
	MEDIA_TYPE=""
	MEDIA_DEVICE=""
	MEDIA_DISTRIBUTIONS=""
	DISTRIB_SUBDIR=""
	TMPDIR=""
	FTP_PATH=""
	NFS_PATH=""
}

# Set the installation media to undefined.
media_reset()
{
	MEDIA_DEVICE=""
	MEDIA_TYPE=""
	MEDIA_DISTRIBUTIONS=""
	FTP_PATH=""
	NFS_PATH=""
	NFS_OPTIONS=""
}

# Set the location of our temporary unpacking directory.
media_set_tmpdir()
{
	if [ "X${TMPDIR}" != "X" ]; then
		return
	fi

	TITLE="Choose temporary directory"
	TMPDIR="/usr/tmp"
	DEFAULT_VALUE="${TMPDIR}"
	if ! input \
"Please specify the name of a directory containing enough free
space to hold the temporary files for this distribution.  At
minimum, a binary distribution will require around 21MB of
temporary space.  At maximum, a src distribution  may take 30MB
or more.  If the directory you specify does not exist, it will
be created for you.  If you do not have enough free space to
hold both the packed and unpacked distribution files, consider
using the NFS or CDROM installation methods as they require no
temporary storage."; then return 1; fi
	TMPDIR=${ANSWER}
	mkdir -p ${TMPDIR}
	return 0
}

media_cd_tmpdir()
{
	if ! cd ${TMPDIR} > /dev/ttyv1 2>&1; then
		error "No such file or directory for ${TMPDIR}, sorry!  Please fix this and try again."
		return 1
	fi
}

media_rm_tmpdir()
{
	cd /
	if [ "X${NO_ASK_REMOVE}" != "X" ]; then
		rm -rf ${_TARGET}
		return
	fi
	if [ -d ${TMPDIR}/${MEDIA_DISTRIBUTION} ]; then
		_TARGET=${TMPDIR}/${MEDIA_DISTRIBUTION}
	else
		_TARGET=${TMPDIR}
	fi
	if dialog --title "Delete contents?" --yesno \
	  "Do you wish to delete ${_TARGET}?" -1 -1; then
		rm -rf ${_TARGET}
		if dialog --title "Future Confirmation?" --yesno \
		  "Do you wish to suppress this dialog in the future?" -1 -1;
		   then
			NO_ASK_REMOVE=yes
		fi
	fi
}

media_select_ftp_site()
{
	dialog --title "Please specify an ftp site" --menu \
"FreeBSD is distributed from a number of sites on the Internet.\n\
Please select the site closest to you or \"other\" if you'd like\n\
to specify another choice.  Also note that not all sites carry\n\
every possible distribution!  Distributions other than the basic\n\
binary set are only guaranteed to be available from the Primary site.\n\
If the first site selected doesn't respond, try one of the alternates.\n\
Please use arrow keys to scroll through all items." \
-1 -1 5 \
  "Primary" "ftp.freebsd.org" \
  "Secondary" "freefall.cdrom.com" \
  "Australia" "ftp.physics.usyd.edu.au" \
  "Finland" "nic.funet.fi" \
  "France" "ftp.ibp.fr" \
  "Germany" "ftp.uni-duisburg.de" \
  "Israel" "orgchem.weizmann.ac.il" \
  "Japan" "ftp.sra.co.jp" \
  "Japan-2" "ftp.mei.co.jp" \
  "Japan-3" "ftp.waseda.ac.jp" \
  "Japan-4" "ftp.pu-toyama.ac.jp" \
  "Japan-5" "ftpsv1.u-aizu.ac.jp" \
  "Japan-6" "tutserver.tutcc.tut.ac.jp" \
  "Japan-7" "ftp.ee.uec.ac.jp" \
  "Korea" "ftp.cau.ac.kr" \
  "Netherlands" "ftp.nl.net" \
  "Russia" "ftp.kiae.su" \
  "Sweden" "ftp.luth.se" \
  "Taiwan" "netbsd.csie.nctu.edu.tw" \
  "Thailand" "ftp.nectec.or.th" \
  "UK" "ftp.demon.co.uk" \
  "UK-2" "src.doc.ic.ac.uk" \
  "UK-3" "unix.hensa.ac.uk" \
  "USA" "ref.tfs.com" \
  "USA-2" "ftp.dataplex.net" \
  "USA-3" "kryten.atinc.com" \
  "USA-4" "ftp.neosoft.com" \
  "other" "None of the above.  I want to specify my own." \
      2> ${TMP}/menu.tmp.$$
	RETVAL=$?
	ANSWER=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval ${RETVAL}; then return 1; fi
   case ${ANSWER} in
   Primary) FTP_PATH="ftp://ftp.freebsd.org/pub/FreeBSD/${DISTNAME}" ;;
   Secondary) FTP_PATH="ftp://freefall.cdrom.com/pub/FreeBSD/${DISTNAME}" ;;
   Australia) FTP_PATH="ftp://ftp.physics.usyd.edu.au/FreeBSD/${DISTNAME}" ;;
   Finland) FTP_PATH="ftp://nic.funet.fi/pub/unix/FreeBSD/${DISTNAME}" ;;
   France) FTP_PATH="ftp://ftp.ibp.fr/pub/FreeBSD/${DISTNAME}" ;;
   Germany) FTP_PATH="ftp://ftp.uni-duisburg.de/pub/unix/FreeBSD/${DISTNAME}" ;;
   Israel) FTP_PATH="ftp://orgchem.weizmann.ac.il/pub/FreeBSD-${DISTNAME}" ;;
   Japan) FTP_PATH="ftp://ftp.sra.co.jp/pub/os/FreeBSD/distribution/${DISTNAME}" ;;
   Japan-2) FTP_PATH="ftp://ftp.mei.co.jp/free/PC-UNIX/FreeBSD/${DISTNAME}" ;;
   Japan-3) FTP_PATH="ftp://ftp.waseda.ac.jp/pub/FreeBSD/${DISTNAME}" ;;
   Japan-4) FTP_PATH="ftp://ftp.pu-toyama.ac.jp/pub/FreeBSD/${DISTNAME}" ;;
   Japan-5) FTP_PATH="ftp://ftpsv1.u-aizu.ac.jp/pub/os/FreeBSD/${DISTNAME}" ;;
   Japan-6) FTP_PATH="ftp://tutserver.tutcc.tut.ac.jp/FreeBSD/FreeBSD-${DISTNAME}" ;;
   Japan-7) FTP_PATH="ftp://ftp.ee.uec.ac.jp/pub/os/FreeBSD.other/FreeBSD-${DISTNAME}" ;;
   Korea) FTP_PATH="ftp://ftp.cau.ac.kr/pub/FreeBSD/${DISTNAME}" ;;
   Netherlands) FTP_PATH="ftp://ftp.nl.net/pub/os/FreeBSD/${DISTNAME}" ;;
   Russia) FTP_PATH="ftp://ftp.kiae.su/FreeBSD/${DISTNAME}" ;;
   Sweden) FTP_PATH="ftp://ftp.luth.se/pub/FreeBSD/${DISTNAME}" ;;
   Taiwan) FTP_PATH="ftp://netbsd.csie.nctu.edu.tw/pub/FreeBSD/${DISTNAME}" ;;
   Thailand) FTP_PATH="ftp://ftp.nectec.or.th/pub/FreeBSD/${DISTNAME}" ;;
   UK) FTP_PATH="ftp://ftp.demon.co.uk/pub/BSD/FreeBSD/${DISTNAME}" ;;
   UK-2) FTP_PATH="ftp://src.doc.ic.ac.uk/packages/unix/FreeBSD/${DISTNAME}" ;;
   UK-3) FTP_PATH="ftp://unix.hensa.ac.uk/pub/walnut.creek/FreeBSD/${DISTNAME}" ;;
   USA) FTP_PATH="ftp://ref.tfs.com/pub/FreeBSD/${DISTNAME}" ;;
   USA-2) FTP_PATH="ftp://ftp.dataplex.net/pub/FreeBSD/${DISTNAME}" ;;
   USA-3) FTP_PATH="ftp://kryten.atinc.com/pub/FreeBSD/${DISTNAME}" ;;
   USA-4) FTP_PATH="ftp://ftp.neosoft.com/systems/FreeBSD/${DISTNAME}" ;;
   other)
	TITLE="FTP Installation Information"
	DEFAULT_VALUE="${FTP_PATH}"
	if ! input \
"Please specify the machine and parent directory location of the
distribution you wish to load.  This should be either a \"URL style\"
specification (e.g. ftp://ftp.freeBSD.org/pub/FreeBSD/) or simply
the name of a host to connect to.  If only a host name is specified,
the installation assumes that you will properly connect and \"mget\"
the files yourself."; then return 1; fi
	FTP_PATH=${ANSWER}
   ;;
   esac
}

media_extract_dist()
{
	if [ ! -f do_cksum.sh ]; then
		if [ -f ${MEDIA_DISTRIBUTION}/do_cksum.sh ]; then
			cd ${MEDIA_DISTRIBUTION}
		fi
	fi
	if [ -f do_cksum.sh ]; then
		message "Verifying checksums for ${MEDIA_DISTRIBUTION} distribution.  Please wait!"
		if sh ./do_cksum.sh; then
			if [ -f extract.sh ]; then
				message "Extracting ${MEDIA_DISTRIBUTION} distribution.  Please wait!"
				if [ -f ./is_interactive ]; then
					sh ./extract.sh
				else
					sh ./extract.sh < /dev/ttyv1 > /dev/ttyv1 2>&1
				fi
				dialog --title "Extraction Complete" --infobox "${MEDIA_DISTRIBUTION} is done" -1 -1
			else
				error "No installation script found!"
			fi
		else
			error "Checksum error(s) found.  Please check media!"
		fi
	else    
		error "Improper ${MEDIA_DISTRIBUTION} distribution.  No checksum script!"
		media_reset
	fi
}

media_install_set()
{
	# check to see if we already have it
	if [ -f ${TMPDIR}/${MEDIA_DISTRIBUTION}/extract.sh ]; then
		cd ${TMPDIR}/${MEDIA_DISTRIBUTION}
		media_extract_dist
		cd /
		return
	fi
	case ${MEDIA_TYPE} in
	cdrom|nfs|ufs|doshd)
		if ! cd ${MEDIA_DEVICE}/${MEDIA_DISTRIBUTION} > /dev/ttyv1 2>&1; then
			error "Unable to cd to ${MEDIA_DEVICE}/${MEDIA_DISTRIBUTION} directory."
			media_reset
		else
			media_extract_dist
			cd /
		fi
		return
	;;

	tape)
		if ! media_set_tmpdir; then return; fi
		if ! media_cd_tmpdir; then return; fi
		if dialog --title "Please mount tape for ${MEDIA_DEVICE}." \
		  --yesno "Please enter the next tape and select\n<Yes> to continue or <No> if finished" -1 -1; then
			message "Loading distribution from ${MEDIA_DEVICE}.\nUse ALT-F2 to see output, ALT-F1 to return."
			if [ "${MEDIA_DEVICE}" = "ftape" ]; then
				progress "${FT_CMD} | ${TAR_CMD} ${TAR_FLAGS} -"
				${FT_CMD} | ${TAR_CMD} ${TAR_FLAGS} - > /dev/ttyv1 2>&1
			else
				progress "${TAR_CMD} ${TAR_FLAGS} ${MEDIA_DEVICE}"
				${TAR_CMD} ${TAR_FLAGS} ${MEDIA_DEVICE} > /dev/ttyv1 2>&1
			fi
		fi
		if [ -d ${MEDIA_DISTRIBUTION} ]; then cd ${MEDIA_DISTRIBUTION}; fi
		media_extract_dist
		media_rm_tmpdir
	;;

	dosfd)
		if ! media_set_tmpdir; then return; fi
		if ! media_cd_tmpdir; then return; fi
		COPYING="yes"
		progress "Preparing to extract from DOS floppies"
		while [ "${COPYING}" = "yes" ]; do
			progress "Asking for DOS diskette"
			if dialog --title "Insert distribution diskette" \
			  --yesno "Please enter the next diskette and select\n<Yes> to continue or <No> if finished" -1 -1; then
				umount ${MNT} > /dev/null 2>&1
				if ! mount_msdos -o ro ${MEDIA_DEVICE} ${MNT}; then
					error "Unable to mount floppy!  Please correct."
				else
					message "Loading distribution from ${MEDIA_DEVICE}.\nUse ALT-F2 to see output, ALT-F1 to return."
					( ${TAR_CMD} -cf - -C ${MNT} . | ${TAR_CMD} -xvf - ) >/dev/ttyv1 2>&1
					umount ${MNT}
				fi
			else
				COPYING="no"
			fi
		done
		media_extract_dist
		media_rm_tmpdir
		return
	;;

	ftp)
		if ! media_set_tmpdir; then return; fi
		if ! media_cd_tmpdir; then return; fi
		if ! echo ${MEDIA_DEVICE} | grep -q -v 'ftp://'; then
			message "Fetching ${MEDIA_DISTRIBUTION} distribution over ftp.\nUse ALT-F2 to see output, ALT-F1 to return."
			mkdir -p ${MEDIA_DISTRIBUTION}
			if ! ncftp ${MEDIA_DEVICE}/${MEDIA_DISTRIBUTION}/* < /dev/null > /dev/ttyv1 2>&1; then
				error "Couldn't fetch ${MEDIA_DISTRIBUTION} distribution from\n${MEDIA_DEVICE}!"
			else
				media_extract_dist
			fi
		else
			dialog --clear
			ftp ${MEDIA_DEVICE}
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
	MEDIA_DISTRIBUTIONS=""
	while [ "${MEDIA_DISTRIBUTIONS}" = "" ]; do

	dialog --title "${DISTNAME}: Choose distributions" \
	--checklist \
"FreeBSD is separated into a number of distributions for ease of\n\
installation.  Please select the distributions you wish to load, any\n\
distributions already marked being MANDATORY - please do not\n\
unselect them!  Please also note that DES (encryption) code is NOT\n\
FOR EXPORT from the U.S.  Please don't endanger U.S. ftp sites by\n\
getting it illegally, thanks!  When finished, select <OK>." \
-1 -1 10 \
  "bin" "Binary base files (mandatory - ${BINSIZE})" ON \
  "games" "Games and other frivolities (${GAMESIZE})" OFF \
  "info" "GNU info files (${INFOSIZE})" OFF \
  "manpages" "Manual pages (${MANSIZE})" OFF \
  "proflibs" "Profiled libraries (${PROFSIZE})" OFF \
  "dict" "Spelling checker dictionary files (${DICTSIZE})" OFF \
  "src" "Sources for all but DES (${SRCSIZE})" OFF \
  "secure" "DES code (and sources) (${SECRSIZE})" OFF \
  "compat1x" "FreeBSD 1.x binary compatability (${COMPATSIZE})" OFF \
  "XFree86-3.1.1" "The XFree86 3.1.1 distribution (${X11SIZE})" OFF \
     2> ${TMP}/menu.tmp.$$
	RETVAL=$?
	MEDIA_DISTRIBUTIONS=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval ${RETVAL}; then return 1; fi
  done
}

media_get_possible_subdir()
{
	if [ -f ${MNT}/${MEDIA_DISTRIBUTION}/extract.sh ]; then return; fi
	DEFAULT_VALUE="${DISTRIB_SUBDIR}"
	TITLE="Distribution Subdirectory"
	if input \
"If the distributions are in a subdirectory of the mount point,
please enter it here (no leading slash - it should be relative
to the mount point).  The directory you enter should be the
*parent* directory of any distribution subdirectories."; then
		if [ "${ANSWER}" != "" ]; then
			MEDIA_DEVICE=${MEDIA_DEVICE}/${ANSWER}
			DISTRIB_SUBDIR=${ANSWER}
		fi
	else
		return 1
	fi
}

# Get values into $MEDIA_TYPE and $MEDIA_DEVICE.  Call network initialization
# if necessary.
media_chose_method()
{
	while [ "${MEDIA_DEVICE}" = "" ]; do

	dialog --title "Installation From" \
--menu \
"Before installing a distribution, you need to chose and/or configure\n\
a method of installation.  Please pick from one of the following options.\n\
If none of the listed options works for you, then your best bet may be to\n\
simply press ESC twice to get a subshell and proceed manually on your own.\n\
If you are already finished with the installation process, select cancel\n\
to proceed." -1 -1 7 \
	"?Kern" "Please show me the kernel boot messages again!" \
	"Tape" "Load distribution from SCSI, QIC-02 or floppy tape" \
	"CDROM" "Load distribution from SCSI or Mitsumi CDROM" \
	"DOS" "Load from DOS floppies or a DOS hard disk partition" \
	"FTP" "Load distribution using FTP" \
	"UFS" "Load the distribution from existing UFS partition" \
	"NFS" "Load the distribution over NFS" 2> ${TMP}/menu.tmp.$$
	RETVAL=$?
	CHOICE=`cat ${TMP}/menu.tmp.$$`
	rm -f ${TMP}/menu.tmp.$$
	if ! handle_rval ${RETVAL}; then return 1; fi

	case ${CHOICE} in
	?Kern)
		if dmesg > ${TMP}/dmesg.out; then
			dialog --title "Kernel boot message output" \
			  --textbox ${TMP}/dmesg.out 22 76
		else
			error "Couldn't get dmesg information! :-("
		fi
	;;

	Tape)
		dialog --title "Choose Tape Type" --menu \
"Which type of tape drive do you have attached to your \n\
system?  FreeBSD supports the following types:\n" -1 -1 3 \
		"SCSI" "SCSI tape drive attached to supported SCSI controller" \
		"QIC-02" "QIC-02 tape drive (Colorado Jumbo, etc)" \
		"floppy" "Floppy tape drive (QIC-40/QIC-80)" 2> ${TMP}/menu.tmp.$$
		RETVAL=$?
		CHOICE=`cat ${TMP}/menu.tmp.$$`
		rm -f ${TMP}/menu.tmp.$$
		if ! handle_rval ${RETVAL}; then continue; fi
		MEDIA_TYPE=tape;
		case ${CHOICE} in
			SCSI)
				DEFAULT_VALUE="/dev/rst0"
				TITLE="SCSI Tape Device"
				if input \
"If you only have one tape drive, simply press return - the
default value should be correct.  Otherwise, enter the
correct value and press return."; then
					MEDIA_DEVICE=${ANSWER}
				fi
			;;

			QIC-02)
				DEFAULT_VALUE="/dev/rwt0"
				TITLE="QIC-02 Tape Device"
				if input \
"If you only have one tape drive, simply press return - the
default value should be correct.  Otherwise, enter the
correct value and press return."; then
					MEDIA_DEVICE=${ANSWER}
				fi
			;;

			floppy)
				MEDIA_DEVICE=ftape
			;;
		esac
	;;

	CDROM)
		dialog --title "Choose CDROM Type" --menu \
"Which type of CDROM drive do you have attached to your \n\
system?  FreeBSD supports the following types:\n" -1 -1 2 \
		"SCSI" "SCSI CDROM drive attached to supported SCSI controller" \
		"Sony" "Sony CDU33 or compatible CDROM drive" \
		"Mitsumi" "Mitsumi CDROM (non-IDE) drive" \
			2> ${TMP}/menu.tmp.$$
		RETVAL=$?
		CHOICE=`cat ${TMP}/menu.tmp.$$`
		rm -f ${TMP}/menu.tmp.$$
		if ! handle_rval ${RETVAL}; then continue; fi
		MEDIA_TYPE=cdrom;
		case ${CHOICE} in
			SCSI)
				MEDIA_DEVICE=/dev/cd0a
			;;

			Sony)
				MEDIA_DEVICE=/dev/scd0a
			;;

			Mitsumi)
				MEDIA_DEVICE=/dev/mcd0a
			;;
		esac
		umount ${MNT} > /dev/null 2>&1
		if ! mount_cd9660 ${MEDIA_DEVICE} ${MNT} > /dev/ttyv1 2>&1; then
			error "Unable to mount ${MEDIA_DEVICE} on ${MNT}"
			MEDIA_DEVICE=""
		else
			MEDIA_DEVICE=${MNT}
			media_get_possible_subdir
			return 0
		fi
	;;

	DOS)
		DEFAULT_VALUE="/dev/fd0"
		if input \
"Please specify the device pointing at your DOS partition or
floppy media.  For a hard disk, this might be something like
/dev/wd0h or /dev/sd0h (as identified in the disklabel editor).
For the "A" floppy drive, it's /dev/fd0, for the "B" floppy
drive it's /dev/fd1\n"; then
			MEDIA_DEVICE=${ANSWER}
			if echo ${MEDIA_DEVICE} | grep -q -v fd; then
				umount ${MNT} > /dev/null 2>&1
				if ! mount_msdos ${MEDIA_DEVICE} ${MNT} > /dev/ttyv1 2>&1; then
					error "Unable to mount ${MEDIA_DEVICE}"
					MEDIA_DEVICE=""
				else
					MEDIA_TYPE=doshd
					MEDIA_DEVICE=${MNT}
					media_get_possible_subdir
					return 0
				fi
			else
				MEDIA_TYPE=dosfd
				return 0
			fi
		fi
	;;

	FTP)
		if ! network_setup; then continue; fi
		if media_select_ftp_site; then
			MEDIA_TYPE=ftp
			MEDIA_DEVICE=${FTP_PATH}
			return 0
		fi
	;;

	NFS)
		if ! network_setup; then continue; fi
		TITLE="NFS Installation Information"
		DEFAULT_VALUE="${NFS_PATH}"
		if ! input \
"Please specify a machine and directory mount point for the
distribution you wish to load.  This must be in machine:dir
format (e.g. zooey:/a/FreeBSD/${DISTNAME}).  The remote
directory *must* be be exported to your machine (or globally)
for this to work!\n"; then continue; fi
		NFS_PATH=${ANSWER}

		DEFAULT_VALUE="${NFS_OPTIONS}"
		if input \
"Do you wish to specify any options to NFS?  If you're installing
from a Sun 4.1.x system, you may wish to specify \`-o resvport' to send
NFS requests over a privileged port (use this if you get nasty
\`\`credential too weak'' errors from the server).  When using a slow
ethernet card or network link, \`-o -r=1024,-w=1024' may also prove helpful.
Options, if any, should be separated by commas."; then
			if [ "${ANSWER}" != "" ]; then
				NFS_OPTIONS="${ANSWER}"
			fi
		fi
		MEDIA_TYPE=nfs
		umount ${MNT} > /dev/null 2>&1
		if ! mount_nfs ${NFS_OPTIONS} ${NFS_PATH} ${MNT} > /dev/ttyv1 2>&1; then
			error "Unable to mount ${NFS_PATH}"
		else
			message "${NFS_PATH} mounted successfully"
			MEDIA_DEVICE=${MNT}
			media_get_possible_subdir
			return 0
		fi
	;;

	UFS)
		dialog --title "User Intervention Requested" --msgbox "
Please mount the filesystem you wish to use somewhere convenient and
exit the shell when you're through.  I'll ask you for the location
of the distribution's parent directory when we come back." -1 -1
		dialog --clear
		/stand/sh
		TITLE="Please enter directory"
		DEFAULT_VALUE="${MNT}"
		if input "Ok, now give me the full pathname of the parent directorys for the distribution(s)."; then
			MEDIA_TYPE=ufs
			MEDIA_DEVICE=${ANSWER}
			return 0
		fi
	;;
	esac
	done
}
