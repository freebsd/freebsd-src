# $FreeBSD$
# $Whistle: Makefile,v 1.4 1999/01/17 03:41:02 julian Exp $

LIB=		netgraph
WARNS?=		2
MAN=		netgraph.3

SHLIB_MAJOR=	1
#SHLIB_MINOR=	1

SRCS=		sock.c msg.c debug.c
INCS=		netgraph.h

MLINKS+=	netgraph.3 NgMkSockNode.3
MLINKS+=	netgraph.3 NgNameNode.3
MLINKS+=	netgraph.3 NgSendMsg.3
MLINKS+=	netgraph.3 NgSendAsciiMsg.3
MLINKS+=	netgraph.3 NgSendMsgReply.3
MLINKS+=	netgraph.3 NgRecvMsg.3
MLINKS+=	netgraph.3 NgRecvAsciiMsg.3
MLINKS+=	netgraph.3 NgSendData.3
MLINKS+=	netgraph.3 NgRecvData.3
MLINKS+=	netgraph.3 NgSetDebug.3
MLINKS+=	netgraph.3 NgSetErrLog.3

.include <bsd.lib.mk>
