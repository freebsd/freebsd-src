#	@(#)Makefile	5.10 (Berkeley) 5/6/91

LIB=	util
# kvm.c needs -I/sys
CFLAGS+=-DLIBC_SCCS -I/sys
SRCS=	daemon.c getloadavg.c kvm.c login.c login_tty.c logout.c logwtmp.c \
	pty.c pwcache.c
MAN3=	getloadavg.3 pwcache.3

MLINKS+=pwcache.3 user_from_uid.3 pwcache.3 group_from_gid.3

.include <bsd.lib.mk>
