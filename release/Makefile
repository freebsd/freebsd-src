#	$Id$
#

MTREE_DIR=	${.CURDIR}/../etc/mtree

hieriarchy:
	# If this exits with an error code of 2 your file hieriarchy was
	# modified for some reason, please read the mtree output to understand
	# what was changed, then run the command again.
	#
	# XXX This will currently error out if you have a procfs mounted,
	# unmount it to get past this problem until procfs is fixed.
	#
	mtree -deu -f ${MTREE_DIR}/BSD.root.dist  -p ${DESTDIR}/
	mtree -deu -f ${MTREE_DIR}/BSD.var.dist   -p ${DESTDIR}/var
	mtree -deu -f ${MTREE_DIR}/BSD.usr.dist   -p ${DESTDIR}/usr
.if defined(MAKE_LOCAL)
	mtree -deu -f ${MTREE_DIR}/BSD.local.dist -p ${DESTDIR}/usr/local
.endif
	cd ${DESTDIR}/ && rm -f ${DESTDIR}/sys && ln -s usr/src/sys sys
