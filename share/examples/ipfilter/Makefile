# $FreeBSD$

NOOBJ=	noobj
BINDIR=	/usr/share/examples

FILES=	README

# dist sample files
DISTFILES= BASIC.NAT BASIC_1.FW BASIC_2.FW \
	example.1 example.2 example.3 example.4 example.5 \
	example.6 example.7 example.8 example.9 example.10 \
	example.11 example.12 example.13 example.sr firewall \
	ftp-proxy ftppxy nat-setup nat.eg server tcpstate

# ftp://ftp.OpenBSD.org/pub/OpenBSD/src/share/ipf/ sample files.
FILES+=	example.14 firewall.1 firewall.2 \
	ipf.conf.permissive ipf.conf.restrictive \
	ipf.conf.sample ipnat.conf.sample

# http://www.obfuscation.org/ipf/ how-to
FILES+=	ipf-howto.txt

# http://coombs.anu.edu.au/~avalon/ sample files
FILES+=	examples.txt rules.txt

beforeinstall:
	cd ${.CURDIR}/../../../contrib/ipfilter/rules; \
	    ${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${DISTFILES} ${DESTDIR}${BINDIR}/ipfilter
	cd ${.CURDIR}; \
	    ${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${FILES} ${DESTDIR}${BINDIR}/ipfilter

.include <bsd.prog.mk>
