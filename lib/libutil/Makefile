#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	util
SHLIB_MAJOR= 2
SHLIB_MINOR= 2
CFLAGS+=-Wall -DLIBC_SCCS -I${.CURDIR} -I/sys
SRCS=	login.c login_tty.c logout.c logwtmp.c pty.c setproctitle.c \
	login_cap.c login_class.c login_auth.c login_times.c login_ok.c
MAN3+=	login.3 login_tty.3 logout.3 logwtmp.3 pty.3 setproctitle.3 \
	login_cap.3 login_class.3 login_times.3 login_ok.3
MAN5+=	login.conf.5
MLINKS+= pty.3 openpty.3  pty.3 forkpty.3
MLINKS+=login_cap.3 login_getclassbyname.3 login_cap.3 login_close.3 \
	login_cap.3 login_getclass.3 login_cap.3 login_getuserclass.3 \
	login_cap.3 login_getcapstr.3 login_cap.3 login_getcaplist.3 \
	login_cap.3 login_getstyle.3 login_cap.3 login_getcaptime.3 \
	login_cap.3 login_getcapnum.3 login_cap.3 login_getcapsize.3 \
	login_cap.3 login_getcapbool.3 login_cap.3 login_getpath.3
MLINKS+=login_class.3 setusercontext.3 login_class.3 setclasscontext.3 \
	login_class.3 setclassenvironment.3 login_class.3 setclassresources.3
MLINKS+=login_times.3 parse_lt.3 login_times.3 in_ltm.3 \
	login_times.3 in_lt.3 login_times.3 in_ltms.3 \
	login_times.3 in_lts.3
MLINKS+=login_ok.3 auth_ttyok.3 login_ok.3 auth_hostok.3 \
	login_ok.3 auth_timeok.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/libutil.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
