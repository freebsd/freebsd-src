# Copyright (c) 2003-2004 Poul-Henning Kamp.
# Copyright (c) 2004      Thomas Quinot.
#
# See /usr/share/examples/etc/bsd-style-copyright for license terms.
#
# $FreeBSD$

.include "${.CURDIR}/make.conf"

# parallism flag for make.
MAKEJ?="-j12"

# Some CF cards behave as slaves, (eg Lexar) and are assigned ad1.
DEVICE?=	ad0
ROOTSLICE=	${DEVICE}s1a
CONFSLICE=	${DEVICE}s3

# Physical disk paramters.  Use diskinfo(8) on the target platform
# to find the correct numbers.  We assume 512 bytes sectors.
HD?=16
SC?=32
SECTS?=501760

# Number of sectors in the data slice (ad0s3).  The rest of the disk
# will be split evenly between the two image slices (ad0s1/ad0s2)
DATASLICE?=10240

# You probably do not need to change these.
WORLDDIR?= ${.CURDIR}/../../..
WD?=	${.OBJDIR}/_.w

# Set this to the name of a script if you want to customize the
# contents of the image filesystems.
#CUSTOMIZE?=		${.CURDIR}/somescript.sh
#CUSTOMIZE_ARGS?=	some_arguments

# Set this to the name of a directory used to populate the
# configuration slice.
#CFGMASTER?=	${.CURDIR}/cfgmaster

.for cust in CUSTOMIZE CFGMASTER
.if !empty(${cust})
.if exists(${.CURDIR}/${${cust}})
${cust}_PATH=	${.CURDIR}/${${cust}}
.elif exists(${${cust}})
${cust}_PATH=	${${cust}}
.endif
.endif
.endfor

#
# The final resulting image is in ${.OBJDIR}/_.i and the single slice
# image in ${.OBJDIR}/_.i.s1
#

# Main target
all:	buildworld installworld buildimage

#
# This is where you customizations to the image can be performed
# Please make sure to do everything relative to ${WD} here.
#
Customize:	_.cs
_.cs:	_.iw _.di _.ik _.di
	echo "/dev/${ROOTSLICE} / ufs ro 1 1" >  ${WD}/etc/fstab
.if empty(CUSTOMIZE_PATH)
	# useful stuff for diskless boot
	sed -i "" -e /beastie/d ${WD}/boot/loader.rc
	sed -i "" -e /ttyd0/s/off/on/ ${WD}/etc/ttys
	echo " -h" > ${WD}/boot.config
.else
	sh -e ${CUSTOMIZE_PATH} ${WD} ${WORLDDIR} ${.CURDIR} ${CUSTOMIZE_ARGS}
.endif
	touch _.cs

###########################################################################
#
# The rest is pretty (or ugly if you prefer) magic, and should generally
# not need to be fiddled with.  Good luck if you need to venture past this
# point.
#

# Run a buildworld with our private make.conf
buildworld:	_.bw
_.bw:
	(cd ${WORLDDIR} && \
		make ${MAKEJ} -s buildworld __MAKE_CONF=${.CURDIR}/make.conf \
		) > _.bw.tmp 2>&1
	mv _.bw.tmp _.bw

# Run installworld and install into our object directory
installworld:	_.iw
_.iw:	_.bw
	-rm -rf ${WD} > /dev/null 2>&1
	-chflags -R noschg ${WD} > /dev/null 2>&1
	rm -rf ${WD}
	mkdir -p ${WD}
	(cd ${WORLDDIR} && \
		make ${MAKEJ} -s installworld \
		DESTDIR=${WD} \
		__MAKE_CONF=${.CURDIR}/make.conf \
		) > _.iw.tmp 2>&1
	mv _.iw.tmp _.iw

# Run distribution target in /etc.
_.di:	_.iw
	mkdir -p ${WD}
	(cd ${WORLDDIR}/etc && \
		make ${MAKEJ} -s distribution \
		DESTDIR=${WD} \
		__MAKE_CONF=${.CURDIR}/make.conf \
		) > _.di.tmp 2>&1
	mv _.di.tmp _.di

# Build kernel
_.bk:
	(cd ${WORLDDIR} && \
		make ${MAKEJ} -s buildkernel \
		__MAKE_CONF=${.CURDIR}/make.conf \
		) > _.bk.tmp 2>&1
	mv _.bk.tmp _.bk

# Install kernel and hints file
_.ik:	_.bk _.di
	cp ${WORLDDIR}/sys/i386/conf/GENERIC.hints ${WD}/boot/device.hints
	(cd ${WORLDDIR} && \
		make ${MAKEJ} installkernel \
		DESTDIR=${WD} \
		__MAKE_CONF=${.CURDIR}/make.conf \
		) > _.ik.tmp 2>&1
	mv _.ik.tmp _.ik


# Create a disk image from the installed image
buildimage:	_.md
_.md:	_.cs
	-umount ${WD}/dev > /dev/null 2>&1
	chflags -R noschg ${WD} > /dev/null 2>&1
	rm -rf ${WD}/var/* ${WD}/dev/* ${WD}/tmp
	# create trigger file for etc/rc.d/initdiskless
	touch ${WD}/etc/diskless
	mkdir -p ${WD}/conf/base/var
	mtree -deU -f ${WD}/etc/mtree/BSD.var.dist -p ${WD}/conf/base/var
	mkdir -p ${WD}/conf/base/etc
	( cd ${WD}/etc && find . -print | cpio -dumpl ../conf/base/etc )
	mkdir -p ${WD}/conf/default/etc
	echo "mount -o ro /dev/${CONFSLICE}" > ${WD}/conf/default/etc/remount
	ln -s var/tmp ${WD}/tmp
	mtree -deU -f ${WD}/etc/mtree/BSD.root.dist -p ${WD}/
	mtree -deU -f ${WD}/etc/mtree/BSD.usr.dist -p ${WD}/usr
	mtree -deU -f ${WD}/etc/mtree/BSD.include.dist -p ${WD}/usr/include
.if exists(${WD}/etc/mtree/BSD.sendmail.dist)
	mtree -deU -f ${WD}/etc/mtree/BSD.sendmail.dist -p ${WD}/
.endif
	(cd ${WD} && mtree -c -Kmd5digest ) > _.mtree.tmp
	sh -e ${.CURDIR}/i386.diskimage \
		${SECTS} \
		${HD} \
		${SC} \
		${DATASLICE} \
		${WD} ${.OBJDIR}/_.i \
		${CFGMASTER_PATH} \
		> _.md.tmp 2>&1
	mv _.mtree.tmp _.mtree
	mv _.md.tmp _.md

clean:
	-rm -rf _.* > /dev/null 2>&1
	-chflags -R noschg _.* > /dev/null 2>&1
	rm -rf _.*

.include <bsd.obj.mk>
