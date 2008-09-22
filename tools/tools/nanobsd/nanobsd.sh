#!/bin/sh
#
# Copyright (c) 2005 Poul-Henning Kamp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

set -e

#######################################################################
#
# Setup default values for all controlling variables.
# These values can be overridden from the config file(s)
#
#######################################################################

# Name of this NanoBSD build.  (Used to construct workdir names)
NANO_NAME=full

# Source tree directory
NANO_SRC=/usr/src

# Where nanobsd additional files live under the source tree
NANO_TOOLS=tools/tools/nanobsd

# Where cust_pkg() finds packages to install
NANO_PACKAGE_DIR=${NANO_SRC}/${NANO_TOOLS}/Pkg

# Object tree directory
# default is subdir of /usr/obj
# XXX: MAKEOBJDIRPREFIX handling... ?
#NANO_OBJ=""

# The directory to put the final images
# default is ${NANO_OBJ}
#NANO_DISKIMGDIR=""

# Parallel Make
NANO_PMAKE="make -j 3"

# Options to put in make.conf during buildworld only
CONF_BUILD=' '

# Options to put in make.conf during installworld only
CONF_INSTALL=' '

# Options to put in make.conf during both build- & installworld.
CONF_WORLD=' '

# Kernel config file to use
NANO_KERNEL=GENERIC

# Customize commands.
NANO_CUSTOMIZE=""

# Late customize commands.
NANO_LATE_CUSTOMIZE=""

# Newfs paramters to use
NANO_NEWFS="-b 4096 -f 512 -i 8192 -O1 -U"

# The drive name of the media at runtime
NANO_DRIVE=ad0

# Target media size in 512 bytes sectors
NANO_MEDIASIZE=1000000

# Number of code images on media (1 or 2)
NANO_IMAGES=2

# 0 -> Leave second image all zeroes so it compresses better.
# 1 -> Initialize second image with a copy of the first
NANO_INIT_IMG2=1

# Size of code file system in 512 bytes sectors
# If zero, size will be as large as possible.
NANO_CODESIZE=0

# Size of configuration file system in 512 bytes sectors
# Cannot be zero.
NANO_CONFSIZE=2048

# Size of data file system in 512 bytes sectors
# If zero: no partition configured.
# If negative: max size possible
NANO_DATASIZE=0

# Size of the /etc ramdisk in 512 bytes sectors
NANO_RAM_ETCSIZE=10240

# Size of the /tmp+/var ramdisk in 512 bytes sectors
NANO_RAM_TMPVARSIZE=10240

# Media geometry, only relevant if bios doesn't understand LBA.
NANO_SECTS=63
NANO_HEADS=16

# boot0 flags/options and configuration
NANO_BOOT0CFG="-o packet -s 1 -m 3"
NANO_BOOTLOADER="boot/boot0sio"

# Backing type of md(4) device
# Can be "file" or "swap"
NANO_MD_BACKING="file"

#######################################################################
# Not a variable at this time

NANO_ARCH=i386

#######################################################################
#
# The functions which do the real work.
# Can be overridden from the config file(s)
#
#######################################################################

clean_build ( ) (
	echo "## Clean and create object directory (${MAKEOBJDIRPREFIX})"

	if rm -rf ${MAKEOBJDIRPREFIX} > /dev/null 2>&1 ; then
		true
	else
		chflags -R noschg ${MAKEOBJDIRPREFIX}
		rm -rf ${MAKEOBJDIRPREFIX}
	fi
	mkdir -p ${MAKEOBJDIRPREFIX}
	printenv > ${MAKEOBJDIRPREFIX}/_.env
)

make_conf_build ( ) (
	echo "## Construct build make.conf ($NANO_MAKE_CONF)"

	echo "${CONF_WORLD}" > ${NANO_MAKE_CONF}
	echo "${CONF_BUILD}" >> ${NANO_MAKE_CONF}
)

build_world ( ) (
	echo "## run buildworld"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.bw"

	cd ${NANO_SRC}
	${NANO_PMAKE} __MAKE_CONF=${NANO_MAKE_CONF} buildworld \
		> ${MAKEOBJDIRPREFIX}/_.bw 2>&1
)

build_kernel ( ) (
	echo "## build kernel ($NANO_KERNEL)"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.bk"

	if [ -f ${NANO_KERNEL} ] ; then
		cp ${NANO_KERNEL} ${NANO_SRC}/sys/${NANO_ARCH}/conf
	fi

	(cd ${NANO_SRC};
	# unset these just in case to avoid compiler complaints
	# when cross-building
	unset TARGET_CPUTYPE
	unset TARGET_BIG_ENDIAN
	${NANO_PMAKE} buildkernel \
		__MAKE_CONF=${NANO_MAKE_CONF} KERNCONF=`basename ${NANO_KERNEL}` \
		> ${MAKEOBJDIRPREFIX}/_.bk 2>&1
	)
)

clean_world ( ) (
	echo "## Clean and create world directory (${NANO_WORLDDIR})"
	if rm -rf ${NANO_WORLDDIR}/ > /dev/null 2>&1 ; then
		true
	else
		chflags -R noschg ${NANO_WORLDDIR}/
		rm -rf ${NANO_WORLDDIR}/
	fi
	mkdir -p ${NANO_WORLDDIR}/
)

make_conf_install ( ) (
	echo "## Construct install make.conf ($NANO_MAKE_CONF)"

	echo "${CONF_WORLD}" > ${NANO_MAKE_CONF}
	echo "${CONF_INSTALL}" >> ${NANO_MAKE_CONF}
)

install_world ( ) (
	echo "## installworld"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.iw"

	cd ${NANO_SRC}
	${NANO_PMAKE} __MAKE_CONF=${NANO_MAKE_CONF} installworld \
		DESTDIR=${NANO_WORLDDIR} \
		> ${MAKEOBJDIRPREFIX}/_.iw 2>&1
	chflags -R noschg ${NANO_WORLDDIR}
)

install_etc ( ) (

	echo "## install /etc"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.etc"

	cd ${NANO_SRC}
	${NANO_PMAKE} __MAKE_CONF=${NANO_MAKE_CONF} distribution \
		DESTDIR=${NANO_WORLDDIR} \
		> ${MAKEOBJDIRPREFIX}/_.etc 2>&1
)

install_kernel ( ) (
	echo "## install kernel"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.ik"

	cd ${NANO_SRC}
	${NANO_PMAKE} installkernel \
		DESTDIR=${NANO_WORLDDIR} \
		__MAKE_CONF=${NANO_MAKE_CONF} KERNCONF=`basename ${NANO_KERNEL}` \
		> ${MAKEOBJDIRPREFIX}/_.ik 2>&1
)

run_customize() (

	echo "## run customize scripts"
	for c in $NANO_CUSTOMIZE
	do
		echo "## customize \"$c\""
		echo "### log: ${MAKEOBJDIRPREFIX}/_.cust.$c"
		echo "### `type $c`"
		( $c ) > ${MAKEOBJDIRPREFIX}/_.cust.$c 2>&1
	done
)

run_late_customize() (

	echo "## run late customize scripts"
	for c in $NANO_LATE_CUSTOMIZE
	do
		echo "## late customize \"$c\""
		echo "### log: ${MAKEOBJDIRPREFIX}/_.late_cust.$c"
		echo "### `type $c`"
		( $c ) > ${MAKEOBJDIRPREFIX}/_.late_cust.$c 2>&1
	done
)

setup_nanobsd ( ) (
	echo "## configure nanobsd setup"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.dl"

	(
	cd ${NANO_WORLDDIR}

	# Move /usr/local/etc to /etc/local so that the /cfg stuff
	# can stomp on it.  Otherwise packages like ipsec-tools which
	# have hardcoded paths under ${prefix}/etc are not tweakable.
	if [ -d usr/local/etc ] ; then
		(
		mkdir etc/local
		cd usr/local/etc
		find . -print | cpio -dumpl ../../../etc/local
		cd ..
		rm -rf etc
		ln -s ../../etc/local etc
		)
	fi

	for d in var etc
	do
		# link /$d under /conf
		# we use hard links so we have them both places.
		# the files in /$d will be hidden by the mount.
		# XXX: configure /$d ramdisk size
		mkdir -p conf/base/$d conf/default/$d
		find $d -print | cpio -dumpl conf/base/
	done

	echo "$NANO_RAM_ETCSIZE" > conf/base/etc/md_size
	echo "$NANO_RAM_TMPVARSIZE" > conf/base/var/md_size

	# pick up config files from the special partition
	echo "mount -o ro /dev/${NANO_DRIVE}s3" > conf/default/etc/remount

	# Put /tmp on the /var ramdisk (could be symlink already)
	rmdir tmp || true
	rm tmp || true
	ln -s var/tmp tmp

	) > ${MAKEOBJDIRPREFIX}/_.dl 2>&1
)

setup_nanobsd_etc ( ) (
	echo "## configure nanobsd /etc"

	(
	cd ${NANO_WORLDDIR}

	# create diskless marker file
	touch etc/diskless

	# Make root filesystem R/O by default
	echo "root_rw_mount=NO" >> etc/defaults/rc.conf

	# save config file for scripts
	echo "NANO_DRIVE=${NANO_DRIVE}" > etc/nanobsd.conf

	echo "/dev/${NANO_DRIVE}s1a / ufs ro 1 1" > etc/fstab
	echo "/dev/${NANO_DRIVE}s3 /cfg ufs rw,noauto 2 2" >> etc/fstab
	mkdir -p cfg
	)
)

prune_usr() (

	# Remove all empty directories in /usr 
	find ${NANO_WORLDDIR}/usr -type d -depth -print |
		while read d
		do
			rmdir $d > /dev/null 2>&1 || true 
		done
)

create_i386_diskimage ( ) (
	echo "## build diskimage"
	echo "### log: ${MAKEOBJDIRPREFIX}/_.di"

	(
	echo $NANO_MEDIASIZE $NANO_IMAGES \
		$NANO_SECTS $NANO_HEADS \
		$NANO_CODESIZE $NANO_CONFSIZE $NANO_DATASIZE |
	awk '
	{
		printf "# %s\n", $0

		# size of cylinder in sectors
		cs = $3 * $4

		# number of full cylinders on media
		cyl = int ($1 / cs)

		# output fdisk geometry spec, truncate cyls to 1023
		if (cyl <= 1023)
			print "g c" cyl " h" $4 " s" $3
		else
			print "g c" 1023 " h" $4 " s" $3

		if ($7 > 0) { 
			# size of data partition in full cylinders
			dsl = int (($7 + cs - 1) / cs)
		} else {
			dsl = 0;
		}

		# size of config partition in full cylinders
		csl = int (($6 + cs - 1) / cs)

		if ($5 == 0) {
			# size of image partition(s) in full cylinders
			isl = int ((cyl - dsl - csl) / $2)
		} else {
			isl = int (($5 + cs - 1) / cs)
		}

		# First image partition start at second track
		print "p 1 165 " $3, isl * cs - $3
		c = isl * cs;

		# Second image partition (if any) also starts offset one 
		# track to keep them identical.
		if ($2 > 1) {
			print "p 2 165 " $3 + c, isl * cs - $3
			c += isl * cs;
		}

		# Config partition starts at cylinder boundary.
		print "p 3 165 " c, csl * cs
		c += csl * cs

		# Data partition (if any) starts at cylinder boundary.
		if ($7 > 0) {
			print "p 4 165 " c, dsl * cs
		} else if ($7 < 0 && $1 > c) {
			print "p 4 165 " c, $1 - c
		} else if ($1 < c) {
			print "Disk space overcommitted by", \
			    c - $1, "sectors" > "/dev/stderr"
			exit 2
		}

		# Force slice 1 to be marked active. This is necessary
		# for booting the image from a USB device to work.
		print "a 1"
	}
	' > ${MAKEOBJDIRPREFIX}/_.fdisk

	IMG=${NANO_DISKIMGDIR}/_.disk.full
	MNT=${MAKEOBJDIRPREFIX}/_.mnt
	mkdir -p ${MNT}

	if [ "${NANO_MD_BACKING}" = "swap" ] ; then
		MD=`mdconfig -a -t swap -s ${NANO_MEDIASIZE} -x ${NANO_SECTS} \
			-y ${NANO_HEADS}`
	else
		echo "Creating md backing file..."
		dd if=/dev/zero of=${IMG} bs=${NANO_SECTS}b \
			count=`expr ${NANO_MEDIASIZE} / ${NANO_SECTS}`
		MD=`mdconfig -a -t vnode -f ${IMG} -x ${NANO_SECTS} \
			-y ${NANO_HEADS}`
	fi

	trap "df -i ${MNT} ; umount ${MNT} || true ; mdconfig -d -u $MD" 1 2 15 EXIT

	fdisk -i -f ${MAKEOBJDIRPREFIX}/_.fdisk ${MD}
	fdisk ${MD}
	# XXX: params
	# XXX: pick up cached boot* files, they may not be in image anymore.
	boot0cfg -B -b ${NANO_WORLDDIR}/${NANO_BOOTLOADER} ${NANO_BOOT0CFG} ${MD}
	bsdlabel -w -B -b ${NANO_WORLDDIR}/boot/boot ${MD}s1
	bsdlabel ${MD}s1

	# Create first image
	newfs ${NANO_NEWFS} /dev/${MD}s1a
	mount /dev/${MD}s1a ${MNT}
	df -i ${MNT}
	echo "Copying worlddir..."
	( cd ${NANO_WORLDDIR} && find . -print | cpio -dump ${MNT} )
	df -i ${MNT}
	echo "Generating mtree..."
	( cd ${MNT} && mtree -c ) > ${MAKEOBJDIRPREFIX}/_.mtree
	( cd ${MNT} && du -k ) > ${MAKEOBJDIRPREFIX}/_.du
	umount ${MNT}

	if [ $NANO_IMAGES -gt 1 -a $NANO_INIT_IMG2 -gt 0 ] ; then
		# Duplicate to second image (if present)
		echo "Duplicating to second image..."
		dd if=/dev/${MD}s1 of=/dev/${MD}s2 bs=64k
		mount /dev/${MD}s2a ${MNT}
		for f in ${MNT}/etc/fstab ${MNT}/conf/base/etc/fstab
		do
			sed -i "" "s/${NANO_DRIVE}s1/${NANO_DRIVE}s2/g" $f
		done
		umount ${MNT}

	fi
	
	# Create Config slice
	newfs ${NANO_NEWFS} /dev/${MD}s3
	# XXX: fill from where ?

	# Create Data slice, if any.
	if [ $NANO_DATASIZE -gt 0 ] ; then
		newfs ${NANO_NEWFS} /dev/${MD}s4
		# XXX: fill from where ?
	fi

	if [ "${NANO_MD_BACKING}" = "swap" ] ; then
		echo "Writing out _.disk.full..."
		dd if=/dev/${MD} of=${IMG} bs=64k
	fi

	echo "Writing out _.disk.image..."
	dd if=/dev/${MD}s1 of=${NANO_DISKIMGDIR}/_.disk.image bs=64k
	mdconfig -d -u $MD
	) > ${MAKEOBJDIRPREFIX}/_.di 2>&1
)

last_orders () (
	# Redefine this function with any last orders you may have
	# after the build completed, for instance to copy the finished
	# image to a more convenient place:
	# cp ${MAKEOBJDIRPREFIX}/_.disk.image /home/ftp/pub/nanobsd.disk
)

#######################################################################
#
# Optional convenience functions.
#
#######################################################################

#######################################################################
# Common Flash device geometries
#

FlashDevice () {
	if [ -d ${NANO_TOOLS} ] ; then
		. ${NANO_TOOLS}/FlashDevice.sub
	else
		. ${NANO_SRC}/${NANO_TOOLS}/FlashDevice.sub
	fi
	sub_FlashDevice $1 $2
}


#######################################################################
# Setup serial console

cust_comconsole () (
	# Enable getty on console
	sed -i "" -e /tty[du]0/s/off/on/ ${NANO_WORLDDIR}/etc/ttys

	# Disable getty on syscons devices
	sed -i "" -e '/^ttyv[0-8]/s/	on/	off/' ${NANO_WORLDDIR}/etc/ttys

	# Tell loader to use serial console early.
	echo " -h" > ${NANO_WORLDDIR}/boot.config
)

#######################################################################
# Allow root login via ssh

cust_allow_ssh_root () (
	sed -i "" -e '/PermitRootLogin/s/.*/PermitRootLogin yes/' \
	    ${NANO_WORLDDIR}/etc/ssh/sshd_config
)

#######################################################################
# Install the stuff under ./Files

cust_install_files () (
	cd ${NANO_TOOLS}/Files
	find . -print | grep -v /CVS | cpio -dumpv ${NANO_WORLDDIR}
)

#######################################################################
# Install packages from ${NANO_PACKAGE_DIR}

cust_pkg () (

	# Copy packages into chroot
	mkdir -p ${NANO_WORLDDIR}/Pkg
	cp ${NANO_PACKAGE_DIR}/* ${NANO_WORLDDIR}/Pkg

	# Count & report how many we have to install
	todo=`ls ${NANO_WORLDDIR}/Pkg | wc -l`
	echo "=== TODO: $todo"
	ls ${NANO_WORLDDIR}/Pkg
	echo "==="
	while true
	do
		# Record how many we have now
		have=`ls ${NANO_WORLDDIR}/var/db/pkg | wc -l`

		# Attempt to install more packages
		# ...but no more than 200 at a time due to pkg_add's internal
		# limitations.
		chroot ${NANO_WORLDDIR} sh -c \
			'ls Pkg/*tbz | xargs -n 200 pkg_add -F' || true

		# See what that got us
		now=`ls ${NANO_WORLDDIR}/var/db/pkg | wc -l`
		echo "=== NOW $now"
		ls ${NANO_WORLDDIR}/var/db/pkg
		echo "==="


		if [ $now -eq $todo ] ; then
			echo "DONE $now packages"
			break
		elif [ $now -eq $have ] ; then
			echo "FAILED: Nothing happened on this pass"
			exit 2
		fi
	done
	rm -rf ${NANO_WORLDDIR}/Pkg
)

#######################################################################
# Convenience function:
# 	Register $1 as customize function.

customize_cmd () {
	NANO_CUSTOMIZE="$NANO_CUSTOMIZE $1"
}

#######################################################################
# Convenience function:
# 	Register $1 as late customize function to run just before
#	image creation.

late_customize_cmd () {
	NANO_LATE_CUSTOMIZE="$NANO_LATE_CUSTOMIZE $1"
}

#######################################################################
#
# All set up to go...
#
#######################################################################

usage () {
	(
	echo "Usage: $0 [-b/-k/-w] [-c config_file]"
	echo "	-b	suppress builds (both kernel and world)"
	echo "	-k	suppress buildkernel"
	echo "	-w	suppress buildworld"
	echo "	-i	suppress disk image build"
	echo "	-c	specify config file"
	) 1>&2
	exit 2
}

#######################################################################
# Parse arguments

do_kernel=true
do_world=true
do_image=true

set +e
args=`getopt bc:hkwi $*`
if [ $? -ne 0 ] ; then
	usage
	exit 2
fi
set -e

set -- $args
for i
do
	case "$i" 
	in
	-b)
		shift;
		do_world=false
		do_kernel=false
		;;
	-k)
		shift;
		do_kernel=false
		;;
	-c)
		. "$2"
		shift;
		shift;
		;;
	-h)
		usage
		;;
	-i)
		do_image=false
		;;
	-w)
		shift;
		do_world=false
		;;
	--)
		shift;
		break;
	esac
done

if [ $# -gt 0 ] ; then
	echo "$0: Extraneous arguments supplied"
	usage
fi

#######################################################################
# Setup and Export Internal variables
#
if [ "x${NANO_OBJ}" = "x" ] ; then
	MAKEOBJDIRPREFIX=/usr/obj/nanobsd.${NANO_NAME}/
	NANO_OBJ=${MAKEOBJDIRPREFIX}
else
	MAKEOBJDIRPREFIX=${NANO_OBJ}
fi

if [ "x${NANO_DISKIMGDIR}" = "x" ] ; then
	NANO_DISKIMGDIR=${MAKEOBJDIRPREFIX}
fi

NANO_WORLDDIR=${MAKEOBJDIRPREFIX}/_.w
NANO_MAKE_CONF=${MAKEOBJDIRPREFIX}/make.conf

if [ -d ${NANO_TOOLS} ] ; then
	true
elif [ -d ${NANO_SRC}/${NANO_TOOLS} ] ; then
	NANO_TOOLS=${NANO_SRC}/${NANO_TOOLS}
else
	echo "NANO_TOOLS directory does not exist" 1>&2
	exit 1
fi

export MAKEOBJDIRPREFIX

export NANO_ARCH
export NANO_CODESIZE
export NANO_CONFSIZE
export NANO_CUSTOMIZE
export NANO_DATASIZE
export NANO_DRIVE
export NANO_HEADS
export NANO_IMAGES
export NANO_MAKE_CONF
export NANO_MEDIASIZE
export NANO_NAME
export NANO_NEWFS
export NANO_OBJ
export NANO_PMAKE
export NANO_SECTS
export NANO_SRC
export NANO_TOOLS
export NANO_WORLDDIR
export NANO_BOOT0CFG
export NANO_BOOTLOADER

#######################################################################
# And then it is as simple as that...

if $do_world ; then
	clean_build
	make_conf_build
	build_world
else
	echo "## Skipping buildworld (as instructed)"
fi

if $do_kernel ; then
	build_kernel
else
	echo "## Skipping buildkernel (as instructed)"
fi

clean_world
make_conf_install
install_world
install_etc
setup_nanobsd_etc
install_kernel

run_customize
setup_nanobsd
prune_usr
run_late_customize
if $do_image ; then
	create_${NANO_ARCH}_diskimage
else
	echo "## Skipping image build (as instructed)"
fi
last_orders

echo "# NanoBSD image ${NANO_NAME} completed"
