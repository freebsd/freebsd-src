# locale.mk peter@clari.net.au 7/3/98
#
# location-specific settings
#
#	$Id: bsd.locale.mk,v 1.3 1998/05/20 05:43:01 mph Exp $

.if !defined(LOCALE)
LOCALE=			USA
.endif

.if ${LOCALE} == "Argentina"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.ar.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Australia"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.au.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.au.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.au.freebsd.org/pub/FreeBSD/ \
			ftp://ftp4.au.freebsd.org/pub/FreeBSD/ \
			ftp://ftp5.au.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Brazil"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.br.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.br.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.br.freebsd.org/pub/FreeBSD/ \
			ftp://ftp4.br.freebsd.org/pub/FreeBSD/ \
			ftp://ftp5.br.freebsd.org/pub/FreeBSD/ \
			ftp://ftp6.br.freebsd.org/pub/FreeBSD/ \
			ftp://ftp7.br.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Canada"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.ca.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Czech Republic"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.cz.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Estonia"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.ee.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Finland"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.fi.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "France"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp2.fr.freebsd.org/pub/FreeBSD/ \
			ftp://ftp.fr.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Germany"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.de.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.de.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.de.freebsd.org/pub/FreeBSD/ \
			ftp://ftp4.de.freebsd.org/pub/FreeBSD/ \
			ftp://ftp5.de.freebsd.org/pub/FreeBSD/ \
			ftp://ftp6.de.freebsd.org/pub/FreeBSD/ \
			ftp://ftp7.de.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Holland"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.nl.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Hong Kong"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.hk.super.net/pub/FreeBSD/

.elif ${LOCALE} == "Iceland"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.is.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Ireland"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.ie.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Israel"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.il.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.il.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Japan"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.jp.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.jp.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.jp.freebsd.org/pub/FreeBSD/ \
			ftp://ftp4.jp.freebsd.org/pub/FreeBSD/ \
			ftp://ftp5.jp.freebsd.org/pub/FreeBSD/ \
			ftp://ftp6.jp.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Korea"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.kr.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.kr.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Poland"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.pl.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Portugal"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.pt.freebsd.org/pub/misc/FreeBSD/ \
			ftp://ftp2.pt.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Russia"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.ru.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.ru.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.ru.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "South Africa"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.za.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.za.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.za.freebsd.org/pub/FreeBSD/ \
			ftp://ftp4.za.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Sweden"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.se.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.se.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.se.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "Taiwan"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.tw.freebsd.org/pub/FreeBSD \
			ftp://ftp2.tw.freebsd.org/pub/FreeBSD \
			ftp://ftp3.tw.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "UK"
NEAR_SITE_LIST?=	${LOCAL_SITE_LIST} \
			ftp://ftp.uk.freebsd.org/pub/FreeBSD/ \
			ftp://ftp2.uk.freebsd.org/pub/FreeBSD/ \
			ftp://ftp3.uk.freebsd.org/pub/FreeBSD/ \
			ftp://ftp4.uk.freebsd.org/pub/FreeBSD/

.elif ${LOCALE} == "USA"
# master sites in usa are included by default
.endif
