#
#
# ===================================
# HARP  |  Host ATM Research Platform
# ===================================
#
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
#	@(#) $FreeBSD: src/lib/libatm/Makefile,v 1.4 2000/01/14 07:57:22 rgrimes Exp $
#
#

LIB=	atm
SRCS=	atm_addr.c cache_key.c ioctl_subr.c ip_addr.c ip_checksum.c timer.c
INCS=	libatm.h

LDADD+=	-lmd
DPADD+=	${LIBMD}

.include <bsd.lib.mk>
