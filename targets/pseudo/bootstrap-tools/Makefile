# $FreeBSD$

# building clang is particularly tedious - so avoid re-inventing wheels

all: bootstrap-toolchain

UPDATE_DEPENDFILE= no
.export UPDATE_DEPENDFILE

# we build for the pseudo machine "host"
TARGET_MACHINE= host

HOST_MACHINE!= uname -m
HOST_MACHINE_ARCH!= uname -p
BTOOLSDIR?= ${HOST_OBJTOP}/tools
LEGACY_TOOLS?= ${BTOOLSDIR}/legacy

BSENV= \
	unset MAKEOBJDIR; MAKEOBJDIRPREFIX=${BTOOLSDIR} \
	MAKESYSPATH=${SRCTOP}/tools/build/mk:${SRCTOP}/share/mk \
	TARGET=${HOST_MACHINE} TARGET_ARCH=${HOST_MACHINE_ARCH} \
	WITHOUT_STAGING=1 STAGE_ROOT= BOOTSTRAPPING_TOOLS=1 \
	WORLDTMP=${BTOOLSDIR} LEGACY_TOOLS=${LEGACY_TOOLS}

.if !defined(OSRELDATE)
ord_h= /usr/include/osreldate.h
.if exists(${ord_h})
OSRELDATE!= sed -n '/define.*__FreeBSD_version/{s,^[^0-9]*,,p;q;}' ${ord_h}
.endif
OSRELDATE?= 0
.endif

# need to keep this in sync with src/Makefile.inc1 
BSARGS= 	DESTDIR= \
		BOOTSTRAPPING=${OSRELDATE} \
		SSP_CFLAGS= \
		MK_HTML=no NO_LINT=yes MK_MAN=no \
		-DNO_PIC MK_PROFILE=no -DNO_SHARED \
		-DNO_CPU_CFLAGS MK_WARNS=no MK_CTF=no \
		MK_CLANG_EXTRAS=no MK_CLANG_FULL=no \
		MK_LLDB=no MK_TESTS=no \
		MK_INCLUDES=yes

legacy: .MAKE .META
	mkdir -p ${LEGACY_TOOLS}
	${MAKE} -C ${SRCTOP}/etc distrib-dirs DESTDIR=${BTOOLSDIR} > $@2
	${MAKE} -C ${SRCTOP}/etc distrib-dirs DESTDIR=${LEGACY_TOOLS} > $@
	${BSENV} ${MAKE} -C ${SRCTOP} -f Makefile.inc1 ${BSARGS} $@
	touch $@

bootstrap-tools: legacy
build-tools: bootstrap-tools
cross-tools: build-tools

cross-tools build-tools bootstrap-tools: .MAKE .META
	${BSENV} ${MAKE} -C ${SRCTOP} -f Makefile.inc1 ${BSARGS} $@
	touch $@

# MAKELEVEL=0 so that dirdeps.mk does its thing
# LEGACY_TOOLS lets us use the bootstaped stuff above
# TARGET* is so that MK_CLANG gets set correctly.
BSTCENV= \
	MAKELEVEL=0 \
	MACHINE=host \
	LEGACY_TOOLS=${LEGACY_TOOLS} \
	TARGET=${HOST_MACHINE} \
	TARGET_ARCH=${HOST_MACHINE_ARCH}

BSTCARGS= \
	${BSARGS} \
	BUILD_DIRDEPS=yes \
	-DWITH_STAGING \
	-DWITH_TOOLSDIR

# finally we build toolchain leveraging the above.
bootstrap-toolchain:	.MAKE cross-tools
	${BSTCENV} ${MAKE} -C ${.CURDIR:H:H} ${BSTCARGS} toolchain
