# $FreeBSD$

.include <bsd.own.mk>
.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/lib
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

PROG=	cvs
MAN=	cvs.1 cvs.5

SRCS=	add.c admin.c annotate.c buffer.c \
	checkin.c checkout.c classify.c client.c \
	commit.c create_adm.c cvsrc.c diff.c edit.c entries.c error.c \
	expand_path.c fileattr.c filesubr.c find_names.c \
	hardlink.c hash.c history.c \
	ignore.c import.c lock.c log.c login.c logmsg.c main.c mkmodules.c \
	modules.c myndbm.c no_diff.c parseinfo.c patch.c prepend_args.c \
	rcs.c rcscmds.c \
	recurse.c release.c remove.c repos.c root.c run.c scramble.c \
	server.c stack.c status.c subr.c \
	tag.c update.c vers_ts.c version.c watch.c \
	wrapper.c zlib.c

# gnu must be before lib to pick correct regex.h
CFLAGS+= -I${.CURDIR} -I../lib -DHAVE_CONFIG_H -I${CVSDIR}/src \
	 -I${DESTDIR}/usr/include/gnu \
	 -I${CVSDIR}/lib -I${CVSDIR}/diff -I.

DPADD=	${LIBCVS} ${LIBDIFF} ${LIBGNUREGEX} ${LIBMD} ${LIBCRYPT} ${LIBZ}
LDADD=	${LIBCVS} ${LIBDIFF} -lgnuregex -lmd -lcrypt -lz

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -DHAVE_GSSAPI -DENCRYPTION
LDADD+= -lgssapi -lkrb5 -lhx509 -lasn1 -lcrypto -lroken -lcrypt -lcom_err
DPADD+= ${LIBGSSAPI} ${LIBKRB5} ${LIBHX509} ${LIBASN1} ${LIBCRYPTO} ${LIBROKEN}
DPADD+= ${LIBCRYPT} ${LIBCOM_ERR}
.endif

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
