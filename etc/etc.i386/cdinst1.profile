#!bin/sh
# cdinst1.profile floppy disk /.profile script
PATH=/sbin:/bin:/usr/bin:/usr/sbin:.
export PATH
HOME=/root
export HOME
TERM=pc3
export TERM
TERMCAP="\
pc3|ibmpc3:li#25:co#80:am:bs:bw:eo:cd=\E[J:ce=\E[K:cl=\Ec:cm=\E[%i%2;%2H:\
do=\E[B:ho=\E[;H:nd=\E[C:up=\E[A:so=\E[7m:se=\E[0m:us=\E[4m:ue=\E[0m:\
:ac=l\332q\304k\277x\263j\331m\300w\302u\264v\301t\303n\305:\
:kb=^h:kh=\E[Y:ku=\E[A:kd=\E[B:kl=\E[D:kr=\E[C:"
export TERMCAP

# To bad uname is not availiable here!
#
OPSYSTEM=FreeBSD
export OPSYSTEM
CDROM_TYPE=0
export CDROM_TYPE
CDROM_MOUNT=/cdrom
export CDROM_MOUNT
CDROM_FILESYSTEM=${CDROM_MOUNT}/filesys
export CDROM_FILESYSTEM
CDROM_BINDIST=${CDROM_MOUNT}/tarballs/bindist/bin_tgz.*
export CDROM_BINDIST

echo
echo    "Welcome to ${OPSYSTEM}."
echo

# Lets find the cd rom drive and get it mounted so we have access to
# all of the binaries, this is really ugly, but it uses the minimum
# amount of support code and should always find us a cdrom drive if
# there is one ready to use!

while [ $CDROM_TYPE -eq 0 ]; do		# Begin of cd drive loop

	mount -t isofs /dev/cd0a ${CDROM_MOUNT} >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		CDROM_TYPE=1
		echo "Found and mounted SCSI CD ROM drive /dev/cd0a"
	else
		mount -t isofs /dev/mcd0a ${CDROM_MOUNT} >/dev/null 2>&1 
		if [ $? -eq 0 ]; then
			CDROM_TYPE=2
			echo "Found and mounted Mitsumi CD ROM drive /dev/mcd0a"
		else
			mount -t isofs /dev/mcd1a ${CDROM_MOUNT} >/dev/null 2>&1
			if [ $? -eq 0 ]; then
				CDROM_TYPE=2
				echo "Found and mounted Mitsumi CD ROM drive /dev/mcd1a"
			else
				CDROM_TYPE=0
				echo "No cdrom drive found, are you sure the cd is in the"
				echo "drive and the drive is ready? Press return to make"
				echo -n "another attempt at finding the cdrom drive."
				read resp
			fi
		fi
	fi
done					# End of cd drive loop

# Okay, we now have a cdrom drive and know what device to call it by, and
# it should be mounted, so lets reset our path so we can use all the binaries
# from the cd rom drive and export the set up variables for the install script
#
PATH=/sbin:/bin:/usr/bin:/usr/sbin:${CDROM_FILESYSTEM}/sbin:${CDROM_FILESYSTEM}/bin:${CDROM_FILESYSTEM}/usr/bin:${CDROM_FILESYSTEM}/usr/sbin:.

/install
