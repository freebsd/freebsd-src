#	@(#)Makefile	8.2 (Berkeley) 4/3/94

PROG=	ftp
SRCS=	cmds.c cmdtab.c ftp.c main.c ruserpass.c domacro.c
LINKS=  ${BINDIR}/ftp ${BINDIR}/pftp
MLINKS= ftp.1 pftp.1
CFLAGS+=-DFTP_DATA_BOTTOM=40000 -DFTP_DATA_TOP=44999

.if defined(SOCKS)
CFLAGS+=-DSOCKS
CFLAGS+=-Dconnect=Rconnect -Dgetsockname=Rgetsockname -Dlisten=Rlisten \
	-Daccept=Raccept -Drcmd=Rrcmd -Dbind=Rbind -Dselect=Rselect
LDADD+=	-lsocks
.endif

.include <bsd.prog.mk>
