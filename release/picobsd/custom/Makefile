# PicoBSD Makefile
#
# $Id: Makefile,v 1.1 1999/12/10 17:38:06 grog Exp $
#
# Taken from:
#
# $FreeBSD$
#

# Building a PicoBSD kernel involves four basic operations:
#
# 1.  Build a kernel.  This is done by the 'kernel' target.
# 2.  Build a PicoBSD file system.  This file system is loaded into an
#     MFS at boot time.  This is done by the 'fs' target.
# 3.  Build a single executable file for all the programs, and install it in 
#     the mfs image.  This is done by the 'crunch' target.
# 4.  Build a floppy image containing the MFS image and some helper code
#     to boot it.  This is done by the 'image' target.
# 5.  Copy the image to the floppy.  Done by the 'copy' target.

# You can set the SRC variable which points to your source tree. It's
# /usr/src by default (most people shouldn't change it).
SRC	?=	/usr/src
EDITOR	=	vi

# LANGUAGE     language to be used (en or pl)
LANGUAGE	=	en

# FLOPPY_SIZE  floppy size in KB (default to 1440)
FLOPPY_SIZE	=	1440

# BUILDDIR is this directory
BUILDDIR	=	${.CURDIR}

# These programs are built with non-standard parameters.  To be sure of
# getting them right, we need to rebuild every time; otherwise we might
# trip over the 'make world' objects.
# To do this right, we need to build the objects in a different directory.
# FIXME.
SPECIAL_PROGS	=	../../../usr.bin/login ../../../usr.bin/passwd

# Get default parameters for MFS
.include "mfsinfo"

# MFS information
MFS_NAME	=	fs.PICOBSD
MFS_MOUNTPOINT	=	mmnt
MFS_VNODE	=	/dev/vn0
# /etc/rc for the mfs image.  This reads the real stuff off the floppy
MFS_RC		=	floppy.tree/etc/mfs.rc

FLOPPY_NAME	=	picobsd.bin
FLOPPY_VNODE	=	/dev/vn1
FLOPPY_MOUNTPOINT =	fmnt

# Variables for building kernel
CONF		?=	$(SRC)/sys/i386/conf
CONFFILE	=	PICOBSD${suffix}
COMPILE		=	$(SRC)/sys/compile/$(CONFFILE)
KERNFILE	=	$(COMPILE)/kernel

FDEV		?=	/dev/fd0

# DEVFS is currently broken. Always set this.
#
NO_DEVFS	=	yes

# Should we keep this?
VER	=	0.445

all:  kernel.gz ${MFS_NAME} crunch ${FLOPPY_NAME} copy crunch-2

kernel.gz:	${COMPILE}/kernel
	@echo "--> Compressing kernel"
	cp -p ${COMPILE}/kernel kernel
	strip kernel
	strip --remove-section=.note --remove-section=.comment kernel
	gzip -f -9 -v -n kernel
	@ls -l kernel.gz

# Create the MFS image file
${MFS_NAME}: ${MFS_VNODE} ${MFS_MOUNTPOINT} 
	@(cd ${.CURDIR} && ${MAKE} vnunconfig-mfs 2>&1 > /dev/null)
	@echo "--> Building and mounting MFS image vnode"
	rm -f ${MFS_NAME}
	dd of=${MFS_NAME} if=/dev/zero count=${MFS_SIZE} bs=1k
	awk 'BEGIN {printf "%c%c", 85, 170}' | \
	    dd of=${MFS_NAME} obs=1 seek=510 conv=notrunc  2> /dev/null
	vnconfig -s labels -c ${MFS_VNODE} ${MFS_NAME}
	#@cd ${.CURDIR} && ${MAKE} vnconfig-mfs
	dd if=/boot/boot1 of=${MFS_NAME} conv=notrunc
	disklabel -rw vn0 auto
	newfs -i ${MFS_INODES} -m 0 -p 0 -o space ${MFS_VNODE}c
	mount ${MFS_VNODE} ${MFS_MOUNTPOINT}
	#@cd ${.CURDIR} && ${MAKE} mount-mfs
	cd ${.CURDIR} && ${MAKE} -f Makefile.mfs DESTDIR=${MFS_MOUNTPOINT} LANGUAGE=${LANGUAGE}
	cd ${.CURDIR} && ${MAKE} -f Makefile.mfs devnodes DESTDIR=${MFS_MOUNTPOINT} LANGUAGE=${LANGUAGE}
	MFS_MOUNTPOINT=${MFS_MOUNTPOINT} INIT=${INIT} VER=${VER} BUILDDIR=${BUILDDIR} LANGUAGE=${LANGUAGE}

# Populate the MFS image.
crunch: ${MFS_MOUNTPOINT}
	@(cd ${.CURDIR} && ${MAKE} mount-mfs)
	@echo "--> Populating MFS image"
	# We need to make login with other options
	#for i in ${SPECIAL_PROGS}; do (cd $$i; make clean); done
	@(cd ${.CURDIR}/crunch1 && ${MAKE} SRC=${SRC} INIT=${INIT}; ${MAKE} install MFS_MOUNTPOINT=../${MFS_MOUNTPOINT})
	# who can tell me how these damned help files get in here?
	rm -rf ${MFS_MOUNTPOINT}/sbin/help
	# Other files for the mfs file system
	((cd ${.CURDIR}/mfs; tar cf - .) | (cd ${MFS_MOUNTPOINT}; tar xf -))
	(cd ${MFS_MOUNTPOINT} && chown -R root *)
	@df -ik ${MFS_MOUNTPOINT}
	@(cd ${.CURDIR} && ${MAKE} umount-mfs)
	@fsck -p ${MFS_VNODE}
	@(cd ${.CURDIR} && ${MAKE} vnunconfig-mfs)

# Create and mount the floppy image
${FLOPPY_NAME}:	${FLOPPY_VNODE} ${FLOPPY_MOUNTPOINT} ${MFS_NAME}
	@cd ${.CURDIR} && ${MAKE} vnunconfig-floppy
	@echo "--> Preparing ${FLOPPY_SIZE}kB floppy filesystem"
	dd of=${FLOPPY_NAME} if=/dev/zero count=${FLOPPY_SIZE} bs=1k
	awk 'BEGIN {printf "%c%c", 85, 170}' | \
	                dd of=${FLOPPY_NAME} obs=1 seek=510 conv=notrunc
	#-vnconfig -s labels -c ${FLOPPY_VNODE} ${FLOPPY_NAME}
	@cd ${.CURDIR} && ${MAKE} vnconfig-floppy
	dd if=/boot/boot1 of=${FLOPPY_NAME} conv=notrunc
	disklabel -Brw -b /boot/boot1 -s /boot/boot2 ${FLOPPY_VNODE}c fd${FLOPPY_SIZE}	
	newfs -i ${FLOPPY_INODES} -m 0 -p 0 -o space ${FLOPPY_VNODE}c
	#mount ${FLOPPY_VNODE} ${FLOPPY_MOUNTPOINT}
	@cd ${.CURDIR} && ${MAKE} mount-floppy

copy:	${FLOPPY_MOUNTPOINT}/boot/defaults ../floppy.tree/boot kernel.gz
	@cd ${.CURDIR} && ${MAKE} mount-floppy
	@echo "--> Populating floppy filesystem from ../floppy.tree"
	@echo -- Make a loader
	#cp /boot/loader floppy.tree/boot
	#mv floppy.tree/boot/loader.kz floppy.tree/boot/loader
.if foo
	rm ../floppy.tree/boot/loader
	mv ../floppy.tree/boot/loader.kz ../floppy.tree/boot/loader
.endif
	@echo -- copy base floppy tree
	@echo -- Base files
	@(cd ${.CURDIR}/../floppy.tree ; tar cXhf ${.CURDIR}/floppy.tree.exclude - . ) | \
		(cd ${FLOPPY_MOUNTPOINT} ; tar xf - )
	@kgzip -o ${FLOPPY_MOUNTPOINT}/boot/loader /boot/loader
	@echo -- Now update from our private floppy tree if it exists
	@echo -- Local files in `pwd`/floppy.tree
	@if [ -d ${.CURDIR}/floppy.tree ] ; then \
		(cd ${.CURDIR}/floppy.tree; tar cXhf ../floppy.tree.exclude - .) | (cd ${FLOPPY_MOUNTPOINT} ; tar xf - ); \
	fi
	# We get stuff here that we can't use.  When the transition
	# to Makefile-based build is complete, we can delete this.
	# XXX
	@rm -rf ${FLOPPY_MOUNTPOINT}/etc
	@(cd ${.CURDIR}/floppy.etc; tar czXf ../floppy.tree.exclude ${.OBJDIR}/${FLOPPY_MOUNTPOINT}/etc.tar.gz .)
	@echo -- Generate password file if we don\'t have one on the floppy.
	@echo -- The variable PASSWD is defined in mfsinfo
	@if [ ${PASSWD} = NO ] ; then \
	    cp -p ${FLOPPY_MOUNTPOINT}/etc/master.passwd .; \
	    pwd_mkdb -d . master.passwd; \
	    mv spwd.db ${FLOPPY_MOUNTPOINT}/etc/; \
	    rm pwd.db master.passwd; \
	fi
	@cp kernel.gz ${FLOPPY_MOUNTPOINT}/kernel.gz
	@echo -- Insert the MFS image
	@gzip -9cv ${MFS_NAME} > ${FLOPPY_MOUNTPOINT}/${MFS_NAME}.gz
.if loaderconf
	XXX finish this.
	cp ${SRC}/sys/boot/forth/loader.conf ${FLOPPY_MOUNTPOINT}/boot/defaults
	echo "userconfig_script_load=\"YES\""	>>${FLOPPY_MOUNTPOINT}/boot/defaults/loader.conf
	echo "rootfs_load=\"YES\""		>>${FLOPPY_MOUNTPOINT}/boot/defaults/loader.conf
	echo "rootfs_name=\"${MFS_NAME}\""	>>${FLOPPY_MOUNTPOINT}/boot/defaults/loader.conf
	echo "rootfs_type=\"mfs_root\""		>>${FLOPPY_MOUNTPOINT}/boot/defaults/loader.conf
#	for i in frames screen ; do \
#		cp ${SRC}/share/examples/bootforth/$${i}.4th ${FLOPPY_MOUNTPOINT}/boot/; \
#	done
.endif
	(echo "-> Fixing permissions"; cd ${FLOPPY_MOUNTPOINT}; chown -R root *)
	@cd ${.CURDIR} && ${MAKE} vnunconfig-floppy

floppy:	${FLOPPY_NAME}
	@echo copying ${FLOPPY_NAME} to ${FDEV}c.  This will take about 50 seconds.
	@echo "wait for the message 'safe to remove' before removing the floppy..."
	@dd if=${FLOPPY_NAME} of=${FDEV}c bs=36b >/dev/null
	@echo Safe to remove disk from ${FDEV}
	
floppy2:
	@if [ -d crunch2 ]; then \
	  if [ ! -d crunch2/floppy2 ]; then \
	    (cd crunch2; make); \
	  fi; \
	  echo copying crunch2/floppy to ${FDEV}c.  This will take about 50 seconds.; \
	  cd crunch2/floppy2; tar czf - . | dd of=/dev/rfd0 conv=sync; \
	  echo Safe to remove disk from ${FDEV}; \
	else \
          echo No second floppy; \
	fi
	
# crunch-2: Build second floppy
crunch-2:
	#if [ -d crunch2 ]; then cd crunch2; make; fi		

# Secondary targets

${COMPILE}/kernel: ${CONFFILE}
	@echo "--> Building kernel"
	@grep -q MD_ROOT ${.ALLSRC}; if [ $$? -ne 0 ]; then \
		echo -- '***** Config file must include MD_ROOT'; \
		exit 1; \
	fi
	cp ${.ALLSRC} ${CONF}
	(cd ${CONF}; config ${CONFFILE};)
.if !defined(NO_DEPENDS)
	(cd ${COMPILE}; ${MAKE} depend NO_MODULES=1;)
.endif
	(cd ${COMPILE}; ${MAKE} all NO_MODULES=1)

# Device nodes for the vnode devices
${FLOPPY_VNODE} ${MFS_VNODE}:
	(cd /dev; ./MAKEDEV `basename $@`)

# Directories to make
${MFS_MOUNTPOINT} ${FLOPPY_MOUNTPOINT} ${FLOPPY_MOUNTPOINT}/boot/defaults ../floppy.tree/boot:
	mkdir -p $@

tidy: vnunconfig
	rm -f ${MFS_NAME}  ${MFS_NAME}.gz ${FLOPPY_NAME} *~ Make.log kernel.gz

clean:	tidy
	if [ -d ${.CURDIR}floppy.tree ]; then \
	   (cd ${.CURDIR}/floppy.tree; rm -f boot/loader;) \
	fi
	rm -rf ${MFS_MOUNTPOINT} ${FLOPPY_MOUNTPOINT} crunch1.conf
	(cd ${.CURDIR}/crunch1; make clean)
	if [ -d crunch2 ]; then cd crunch2; make clean; fi
	if [ -d ${COMPILE} ] ; then cd ${COMPILE}; make clean; fi

vnconfig: vnconfig-mfs vnconfig-floppy
vnconfig-mfs: ${MFS_VNODE} vnunconfig-mfs
	vnconfig -s labels -c ${MFS_VNODE} ${MFS_NAME}
vnconfig-floppy: ${FLOPPY_VNODE} vnunconfig-floppy
	vnconfig -s labels -c ${FLOPPY_VNODE} ${FLOPPY_NAME}
vnunconfig: vnunconfig-mfs vnunconfig-floppy
vnunconfig-mfs: ${MFS_VNODE} umount-mfs
	@-vnconfig -u  ${MFS_VNODE}
vnunconfig-floppy: ${FLOPPY_VNODE} umount-floppy
	@-vnconfig -u ${FLOPPY_VNODE}
mount: mount-mfs mount-floppy
mount-mfs: ${MFS_MOUNTPOINT} vnunconfig-mfs vnconfig-mfs
	mount ${MFS_VNODE} ${MFS_MOUNTPOINT}
mount-floppy: ${FLOPPY_MOUNTPOINT} vnunconfig-floppy vnconfig-floppy
	mount ${FLOPPY_VNODE} ${FLOPPY_MOUNTPOINT}
umount: umount-mfs umount-floppy
umount-mfs: ${MFS_VNODE} ${MFS_MOUNTPOINT}
	@-umount ${MFS_VNODE} #${MFS_MOUNTPOINT} 
umount-floppy: ${FLOPPY_VNODE} ${FLOPPY_MOUNTPOINT}
	@-umount ${FLOPPY_VNODE} #${FLOPPY_MOUNTPOINT}

SUBDIR=	crunch1

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
