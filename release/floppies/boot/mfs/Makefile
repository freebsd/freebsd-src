###
#	$Id: Makefile,v 1.1 1996/05/21 01:12:36 julian Exp $
#
# What are we if we weren't told..
CRUNCH?= mfs
CRUNCHDIRS= ../crunch
OBJTOP=../../../..
TOP=${.CURDIR}/../../../..
FS_BIN=${.CURDIR}/../..
FSLABEL=minimum
ZIP=false

# the directories you want on the fs
STANDLINKS=		bin sbin
FS_DIRS=		dev stand etc mnt mnt1 mnt2 tmp
FS_DEVICES= std wd0 wd1 wd2 sd0 sd1 sd2 cuaa0 cuaa1 cuaa2 cuaa3 \
	fd0 fd1 cd0 mcd0 scd0 matcd0 wcd0 st0 ft0 wt0 vty4
# a subdir that contains a verbatim image to be copied to the fs
VERBATIM=	../verbatim

.include "../../crunch_fs.mk"
all: fs_image

#	tar --exclude CVS -cf - -C ${.CURDIR}/sysinstall help | \
#		tar xvf - -C ${RD}/mfsfd/stand

