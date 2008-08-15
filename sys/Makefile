# $FreeBSD$

.include <bsd.own.mk>

# The boot loader
.if ${MK_BOOT} != "no"
SUBDIR=	boot
.endif

# Directories to include in cscope name file and TAGS.
CSCOPEDIRS=	boot bsm cam cddl compat conf contrib crypto ddb dev fs gdb \
		geom gnu isa kern libkern modules net net80211 netatalk \
		netgraph netinet netinet6 netipsec netipx netnatm netncp \
		netsmb nfs nfs4client nfsclient nfsserver nlm opencrypto \
		pccard pci rpc security sys ufs vm xdr ${CSCOPE_ARCHDIR}
.if defined(ALL_ARCH)
CSCOPE_ARCHDIR ?= amd64 arm i386 ia64 mips pc98 powerpc sparc64 sun4v
.else
CSCOPE_ARCHDIR ?= ${MACHINE}
.endif

# Loadable kernel modules

.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

# You need the devel/cscope port for this.
cscope: cscope.out
cscope.out: ${.CURDIR}/cscope.files
	cd ${.CURDIR}; cscope -k -buq -p4

${.CURDIR}/cscope.files: .PHONY
	cd ${.CURDIR}; \
		find ${CSCOPEDIRS} -name "*.[chSs]" -a -type f > ${.TARGET}

cscope-clean:
	rm -f cscope.files cscope.out cscope.in.out cscope.po.out

# You need the devel/global and one of editor/emacs* ports for that.
TAGS ${.CURDIR}/TAGS: ${.CURDIR}/cscope.files
	rm -f ${.CURDIR}/TAGS
	cd ${.CURDIR}; xargs etags -a < ${.CURDIR}/cscope.files

# You need the textproc/glimpse ports for this.
glimpse:
.if !exists(${.CURDIR}/.glimpse_exclude)
	echo .svn > ${.CURDIR}/.glimpse_exclude
	echo /compile/ >> ${.CURDIR}/.glimpse_exclude
.endif
	cd ${.CURDIR}; glimpseindex -H . -B -f -o .

glimpse-clean:
	cd ${.CURDIR}; rm -f .glimpse_*

.include <bsd.subdir.mk>
