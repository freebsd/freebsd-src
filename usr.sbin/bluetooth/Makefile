# $Id: Makefile,v 1.5 2003/09/08 02:28:35 max Exp $

.include <src.opts.mk>

SUBDIR= \
	bluetooth-config \
	btpand \
	hccontrol \
	hcsecd \
	l2control \
	l2ping \
	rfcomm_pppd \
	sdpcontrol \
	sdpd

.if ${MK_USB} != "no"
SUBDIR+=	ath3kfw
SUBDIR+=	bcmfw
SUBDIR+=	bthidcontrol
SUBDIR+=	bthidd
SUBDIR+=	iwmbtfw
SUBDIR+=	rtlbtfw
.endif

.include <bsd.subdir.mk>

