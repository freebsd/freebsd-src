###
#	$Id: Makefile,v 1.214 1996/05/02 08:18:28 jkh Exp $
#
# What are we if we weren't told..
CRUNCH?= fixit
CRUNCHDIRS= ../crunch
TOP=${.CURDIR}/../../../..
FS_BIN=${.CURDIR}/../..

# the directories you want on the fs
FS_DIRS=		dev stand bin sbin etc mnt mnt1 mnt2 mnt3 mnt4 tmp
# a subdir that contains a verbatim image to be copied to the fs
VERBATIM=	../verbatim

.include "../../crunch_fs.mk"
all: fs_image
