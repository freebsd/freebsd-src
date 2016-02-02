# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/usr.sbin/mtree

.PATH: ${TESTSRC}

ATF_TESTS_SH=	nmtree_test
ATF_TESTS_SH_SRC_nmtree_test=	t_mtree.sh

FILESDIR=	${TESTSDIR}

# NOTE: the output from FreeBSD's nmtree displays sha256digest instead of
# sha256; we need to mangle the specfiles to reflect this.
.for f in mtree_d_create.out netbsd6_d_create.out
CLEANFILES+=	$f $f.tmp
FILES+=		$f
$f: ${TESTSRC}/$f
	sed -e 's/sha256/sha256digest/g' < ${.ALLSRC} > ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor

FILES+=		d_convert.in
FILES+=		d_convert_C.out
FILES+=		d_convert_C_S.out
FILES+=		d_convert_D.out
FILES+=		d_convert_D_S.out
FILES+=		d_merge.in
FILES+=		d_merge_C_M.out
FILES+=		d_merge_C_M_S.out

.include <bsd.test.mk>
