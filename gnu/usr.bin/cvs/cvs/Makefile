# $FreeBSD$

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

PROG=	cvs
MAN=	cvs.1 cvs.5

SRCS=	add.c admin.c annotate.c buffer.c checkin.c checkout.c classify.c client.c \
	commit.c create_adm.c cvsrc.c diff.c edit.c entries.c error.c \
	expand_path.c fileattr.c filesubr.c find_names.c \
	hardlink.c hash.c history.c \
	ignore.c import.c lock.c log.c login.c logmsg.c main.c mkmodules.c \
	modules.c myndbm.c no_diff.c parseinfo.c patch.c prepend_args.c \
	rcs.c rcscmds.c \
	recurse.c release.c remove.c repos.c root.c run.c scramble.c \
	server.c status.c subr.c tag.c update.c vers_ts.c version.c watch.c \
	wrapper.c zlib.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -DHAVE_CONFIG_H \
	 -I${CVSDIR}/src -I${CVSDIR}/lib -I${CVSDIR}/diff

DPADD+=	${LIBCVS} ${LIBDIFF} ${LIBGNUREGEX} ${LIBMD} ${LIBCRYPT} ${LIBZ}
LDADD+=	${LIBCVS} ${LIBDIFF} -lgnuregex -lmd -lcrypt -lz

.if defined(MAKE_KERBEROS4) && !defined(NO_OPENSSL) && !defined(NOCRYPT)
CFLAGS+=-DHAVE_KERBEROS -DHAVE_KRB_GET_ERR_TEXT
LDADD+=	-lkrb -lcrypto -lcom_err
DPADD+=	${LIBKRB} ${LIBCRYPTO} ${LIBCOM_ERR}
DISTRIBUTION=	krb4
# XXX do we want to mess with this?  It's probably not really worth it for
# the public freebsd stuff, but others might want it.  It's an optional
# feature anyway, the -x switch is needed to activate it.
CFLAGS+= -DENCRYPTION
.endif

CLEANFILES+=	version.c
VERSION!=	sed < ${CVSDIR}/configure \
		-e '/^VERSION=/!d' -e 's/.*=\(.*\)/\1/' -e q

version.c: version.c.in
	sed -e "s,@VERSION@,${VERSION}-FreeBSD,g" ${.ALLSRC} > ${.TARGET}

#
# Regression test support
#
CLEANDIRS+=cvs-sanity
.ifmake regress
USERID!=id -u
regress:
	mkdir -p ${.OBJDIR}/cvs-sanity/tmp ${.OBJDIR}/cvs-sanity/work
.if ${USERID} == "0"
	chown -R nobody ${.OBJDIR}/cvs-sanity
	(TESTDIR=`sh -c 'cd ${.OBJDIR}/cvs-sanity/tmp && /bin/pwd'`;\
	        export TESTDIR;\
		cd ${.OBJDIR}/cvs-sanity/work;\
		su -m nobody -c "sh ${CVSDIR}/src/sanity.sh ${.OBJDIR}/cvs")
.else
	(TESTDIR=`sh -c 'cd ${.OBJDIR}/cvs-sanity/tmp && /bin/pwd'`;\
		export TESTDIR;\
		cd ${.OBJDIR}/cvs-sanity/work;\
		sh ${CVSDIR}/src/sanity.sh ${.OBJDIR}/cvs)
.endif
.endif

.include <bsd.prog.mk>
