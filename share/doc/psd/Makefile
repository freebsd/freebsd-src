#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.3 1995/01/11 04:24:41 wollman Exp $

# The following modules do not build/install:
# 10.gdb, 13.rcs

# The following modules are encumbered:
# 01.cacm 02.implement 03.iosys 04.uprog 06.Clang 11.adb 14.sccs
# 15.yacc 16.lex 17.m4

# The following modules do not apply to FreeBSD:
# 07.pascal 08.f77 09.f77io

# The following modules are new in FreeBSD:
# 22.rpcgen 23.rpc 24.xdr 25.xdrrfc 26.rpcrfc 27.nfsrfc

# 05.sysman sends nroff into an infinite loop.

BINDIR=	/usr/share/doc/psd
SUBDIR=	title contents 12.make 18.gprof 19.curses 20.ipctut 21.ipc \
	22.rpcgen 23.rpc 24.xdr 25.xdrrfc 26.rpcrfc 27.nfsrpc

.include <bsd.subdir.mk>
