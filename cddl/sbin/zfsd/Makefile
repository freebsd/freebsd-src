# $FreeBSD$

PROG_CXX=	zfsd
SRCS=		callout.cc		\
		case_file.cc		\
		dev_ctl_event.cc	\
		vdev.cc			\
		vdev_iterator.cc	\
		zfsd.cc			\
		zfsd_exception.cc	\
		zpool_list.cc

NO_MAN=		YES

WARNS?=		0

INCFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
INCFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
INCFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/lib/libumem
INCFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
INCFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head
INCFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libuutil/common
INCFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libumem/common
INCFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs/common
INCFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libnvpair
INCFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
INCFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
INCFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
INCFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys

CFLAGS= -g -DNEED_SOLARIS_BOOLEAN ${INCFLAGS}

#NO_SHARED?=	YES

DPADD=  ${LIBZFS} ${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} \
	${LIBNVPAIR} ${LIBUUTIL}
LDADD=  -lzfs -lutil -lgeom -lbsdxml -lsbuf -lnvpair -luutil

#DPADD=  ${LIBAVL} ${LIBZFS} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} \
#        ${LIBM} ${LIBNVPAIR} ${LIBUUTIL} ${LIBUTIL}
#LDADD=  -lavl -lzfs -lgeom -lbsdxml -lsbuf \
#        -lm -lnvpair -luutil -lutil

cscope:
	find ${.CURDIR} -type f -a \( -name "*.[ch]" -o -name "*.cc" \) \
	     > ${.CURDIR}/cscope.files
	cd ${.CURDIR} && cscope -buq ${INCFLAGS}

.include <bsd.prog.mk>
