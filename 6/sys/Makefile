# $FreeBSD$

.if ${MACHINE_ARCH} == "arm"
NO_BOOT=
.endif

# The boot loader
.if !defined(NO_BOOT)
SUBDIR=	boot
.endif

# Directories to include in cscope name file and TAGS.
CSCOPEDIRS=	coda compat conf contrib crypto ddb dev fs gnu i4b isa \
		isofs kern libkern modules net netatalk netatm netgraph \
		netinet netinet6 netipx netkey netnatm netncp netsmb nfs \
		nfsclient nfs4client rpc pccard pci posix4 sys ufs vm \
		${CSCOPE_ARCHDIR}
.if defined(ALL_ARCH)
CSCOPE_ARCHDIR ?= amd64 arm i386 ia64 pc98 powerpc sparc64 sun4v
.else
CSCOPE_ARCHDIR ?= ${MACHINE}
.endif

# Loadable kernel modules

.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

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

.include <bsd.subdir.mk>
