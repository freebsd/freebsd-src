###
#	$Id: Makefile,v 1.4 1997/07/16 12:24:27 julian Exp $
#
# What are we if we weren't told..
CRUNCH?= fixit
CRUNCHDIRS= ../crunch
TOP=${.CURDIR}/../../../..
OBJTOP=${.OBJDIR}/../../../..
SCRIPTDIR=${.CURDIR}/../../bin
#FSLABEL=minimum
FSINODE=2000


# the directories you want on the fs
FS_DIRS=		dev stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp
# a subdir that contains a verbatim image to be copied to the fs
VERBATIM=	../verbatim

.include "../../mk/crunch_fs.mk"
all: fs_image
