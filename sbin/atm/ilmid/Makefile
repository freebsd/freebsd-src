# ===================================
# HARP  |  Host ATM Research Platform
# ===================================
#
# This Host ATM Research Platform ("HARP") file (the "Software") is
# made available by Network Computing Services, Inc. ("NetworkCS")
# "AS IS".  NetworkCS does not provide maintenance, improvements or
# support of any kind.
#
# NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
# INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
# SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
# In no event shall NetworkCS be responsible for any damages, including
# but not limited to consequential damages, arising from or relating to
# any use of the Software or related support.
#
# Copyright 1994-1998 Network Computing Services, Inc.
#
# Copies of this Software may be made, however, the above copyright
# notice must be reproduced on all copies.
#
#	@(#) $Id: Makefile,v 1.5 1998/07/10 16:01:58 jpt Exp $
# $FreeBSD$

PROG=	ilmid
MAN=	ilmid.8

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../../../sys

DPADD=	${LIBATM}
LDADD=	-latm

.include <bsd.prog.mk>
