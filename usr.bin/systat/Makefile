#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <src.opts.mk>

PROG=	systat
SRCS=	cmds.c cmdtab.c devs.c fetch.c iolat.c iostat.c keyboard.c main.c sysput.c \
	netcmds.c netstat.c pigs.c proc.c swap.c icmp.c \
	mode.c ip.c sctp.c tcp.c zarc.c \
	vmstat.c convtbl.c ifcmds.c ifstat.c

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	icmp6.c ip6.c
CFLAGS+= -DINET6
.endif

LIBADD=	tinfow ncursesw m devstat kvm util procstat

.include <bsd.prog.mk>

CFLAGS+= -Wno-cast-align -Wno-cast-qual
