#	from: @(#)Makefile	5.10 (Berkeley) 5/6/91
#	$Id: Makefile,v 1.10 1994/02/23 09:56:37 rgrimes Exp $

LIB=	util
# kvm.c needs -I/sys
CFLAGS+=-DLIBC_SCCS -I/sys
SRCS=	daemon.c getloadavg.c kvm.c login.c login_tty.c logout.c logwtmp.c \
	pty.c pwcache.c config.c osname.c
MAN3=	getloadavg.3 pwcache.3 config_open.3 daemon.3 osname.3

MLINKS+=pwcache.3 user_from_uid.3 \
	pwcache.3 group_from_gid.3 \
	config_open.3 config_close.3 \
	config_open.3 config_next.3 \
	config_open.3 config_skip.3 \
	osname.3 osnamever.3

.include <bsd.lib.mk>
