# $FreeBSD$

.include <bsd.own.mk>

# The boot loader
.if ${MK_BOOT} != "no"
SUBDIR=	boot
.endif

# Directories to include in cscope name file and TAGS.
CSCOPEDIRS=	cam coda compat conf contrib crypto ddb dev fs geom gnu i4b \
		isa isofs kern libkern modules net net80211 netatalk netatm \
		netgraph netinet netinet6 netipx netkey netnatm netncp \
		netsmb nfs nfsclient nfs4client rpc pccard pci sys \
		ufs vm ${ARCHDIR}

ARCHDIR	?=	${MACHINE}

# Loadable kernel modules

.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

cscope:	${.CURDIR}/cscopenamefile
	cd ${.CURDIR}; cscope -k -p4 -i cscopenamefile

${.CURDIR}/cscopenamefile: 
	cd ${.CURDIR}; find ${CSCOPEDIRS} -name "*.[csh]" > ${.TARGET}

TAGS ${.CURDIR}/TAGS:	${.CURDIR}/cscopenamefile
	rm -f ${.CURDIR}/TAGS
	cd ${.CURDIR}; xargs etags -a < ${.CURDIR}/cscopenamefile

.include <bsd.subdir.mk>
