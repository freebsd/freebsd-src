# $FreeBSD$
#	@(#)Make.tags.inc	8.1 (Berkeley) 6/11/93

SYS?=	${.CURDIR}/..

# Common files for "make tags", included by the Makefile for each
# architecture.

# Put the /sys/sys include files at the end so that subroutine definitions
# win when there is a struct tag with the same name (e.g., vmmeter).  The
# better solution would be for ctags to generate "struct vmmeter" tags.

COMM=	${SYS}/dev/advansys/*.[ch] \
	${SYS}/dev/aha/*.[ch] \
	${SYS}/dev/aic7xxx/*.[ch] \
	${SYS}/dev/buslogic/*.[ch] \
	${SYS}/dev/dpt/*.[ch] \
	${SYS}/dev/en/*.[ch] \
	${SYS}/dev/iicbus/*.[ch] \
	${SYS}/dev/isp/*.[ch] \
	${SYS}/dev/pdq/*.[ch] \
	${SYS}/dev/ppbus/*.[ch] \
	${SYS}/dev/smbus/*.[ch] \
	${SYS}/dev/vx/*.[ch] \
	${SYS}/fs/cd9660/*.[ch] \
	${SYS}/fs/deadfs/*.[ch] \
	${SYS}/fs/devfs/*.[ch] \
	${SYS}/fs/fdescfs/*.[ch] \
	${SYS}/fs/fifofs/*.[ch] \
	${SYS}/fs/msdosfs/*.[ch] \
	${SYS}/fs/nullfs/*.[ch] \
	${SYS}/fs/procfs/*.[ch] \
	${SYS}/fs/smbfs/*.[ch] \
	${SYS}/fs/udf/*.[ch] \
	${SYS}/fs/unionfs/*.[ch] \
	${SYS}/geom/*.[ch] \
	${SYS}/kern/*.[ch] \
	${SYS}/net/*.[ch] \
	${SYS}/netatalk/*.[ch] \
	${SYS}/netinet/*.[ch] \
	${SYS}/netinet6/*.[ch] \
	${SYS}/netipsec/*.[ch] \
	${SYS}/netipx/*.[ch] \
	${SYS}/netnatm/*.[ch] \
	${SYS}/nfs/*.[ch] \
	${SYS}/nfsclient/*.[ch] \
	${SYS}/nfsserver/*.[ch] \
	${SYS}/pci/*.[ch] \
	${SYS}/ufs/ffs/*.[ch] \
	${SYS}/ufs/ufs/*.[ch] \
	${SYS}/vm/*.[ch] \
	${SYS}/sys/*.[ch]

COMMDIR1= ${SYS}/conf \
	${SYS}/geom \
	${SYS}/kern \
	${SYS}/net \
	${SYS}/netatalk \
	${SYS}/netinet \
	${SYS}/netinet6 \
	${SYS}/netipsec \
	${SYS}/netipx \
	${SYS}/netnatm \
	${SYS}/nfs \
	${SYS}/pci \
	${SYS}/vm \
	${SYS}/sys

COMMDIR2= ${SYS}/dev/advansys \
	${SYS}/dev/aha \
	${SYS}/dev/aic7xxx \
	${SYS}/dev/buslogic \
	${SYS}/dev/ccd \
	${SYS}/dev/dec \
	${SYS}/dev/dpt \
	${SYS}/dev/en \
	${SYS}/dev/hea \
	${SYS}/dev/hfa \
	${SYS}/dev/iicbus \
	${SYS}/dev/isp \
	${SYS}/dev/pdq \
	${SYS}/dev/ppbus \
	${SYS}/dev/smbus \
	${SYS}/dev/vn \
	${SYS}/dev/vx \
	${SYS}/fs/deadfs \
	${SYS}/fs/devfs \
	${SYS}/fs/fdescfs \
	${SYS}/fs/fifofs \
	${SYS}/fs/msdosfs \
	${SYS}/fs/nullfs \
	${SYS}/fs/procfs \
	${SYS}/fs/specfs \
	${SYS}/fs/unionfs \
	${SYS}/fs/cd9660 \
	${SYS}/ufs/ffs \
	${SYS}/ufs/ufs
