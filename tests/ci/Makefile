# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 The FreeBSD Foundation
#
# This software was developed by Cybermancer Infosec <bofh@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#
# Makefile for CI testing.
#
# User-driven targets:
#  ci: Run CI tests
#  ci-smoke: Run smoke tests which is simply booting the image
#  ci-full: Run full tests
#
# Variables affecting the build process:
#  TARGET/TARGET_ARCH: architecture of built release (default: same as build host)
#  KERNELCONF: kernel configuration to use
#  USE_QEMU: Use QEMU for testing rather than bhyve
#

WORLDDIR?=	${.CURDIR}/../..
RELEASEDIR=	${WORLDDIR}/release
MAKECONF?=	/dev/null
SRCCONF?=	/dev/null
_MEMORY!=sysctl -n hw.physmem 2>/dev/null
PARALLEL_JOBS!=sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null
TOTAL_MEMORY!=expr ${_MEMORY} / 1073741824
KERNCONF?=	GENERIC
LOCALBASE?=	/usr/local

.if !defined(TARGET) || empty(TARGET)
TARGET=		${MACHINE}
.endif
.if !defined(TARGET_ARCH) || empty(TARGET_ARCH)
.if ${TARGET} == ${MACHINE}
TARGET_ARCH=	${MACHINE_ARCH}
.else
TARGET_ARCH=	${TARGET}
.endif
.endif
IMAKE=		${MAKE} TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH}

.if defined(CROSS_TOOLCHAIN) || !empty(CROSS_TOOLCHAIN)
CROSS_TOOLCHAIN_PARAM=	"CROSS_TOOLCHAIN=${CROSS_TOOLCHAIN}"
.endif

# Define OSRELEASE by using newvers.sh
.if !defined(OSRELEASE) || empty(OSRELEASE)
.for _V in TYPE BRANCH REVISION
. if !defined(${_V}) || empty(${_V})
${_V}!=	eval $$(awk '/^${_V}=/{print}' ${.CURDIR}/../../sys/conf/newvers.sh); echo $$${_V}
. endif
.endfor
.for _V in ${TARGET_ARCH}
.if !empty(TARGET:M${_V})
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH}-${TARGET}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-]/_/g}_${TARGET}
.else
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH}-${TARGET}-${TARGET_ARCH}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-]/_/g}_${TARGET_ARCH}
.endif
.endfor
.endif

.if exists(${.CURDIR}/tools/ci.conf) && !defined(CICONF)
CICONF?=	${.CURDIR}/tools/ci.conf
.endif
SWAPSIZE?=	1g
VMFS?=		ufs
FORMAT=		raw
CIIMAGE=	ci-${OSRELEASE}-${GITREV}-${KERNCONF}.${FORMAT}
CIDISK?=	${.OBJDIR}/${CIIMAGE}
VMSIZE?=	6g
CITYPE?=	full
META_TAR!=mktemp /tmp/meta.XXXXXX
META_DIR!=mktemp -d /tmp/meta.XXXXXX
META_DIROUT!=mktemp -d /tmp/meta.XXXXXX
DISC_CAM!=truncate -s 128m /tmp/disk-cam
EXTRA_DISK_NUM?=5
DISK_NUMBERS!=jot - 1 ${EXTRA_DISK_NUM}
BHYVE_EXTRA_DISK_PARAM?=
BHYVE_EXTRA_DISK_PARAM+=-s 4:0,ahci-hd,/tmp/disk-cam
.for i in ${DISK_NUMBERS}
NEW_DISK!=truncate -s 128m /tmp/disk${i}
BHYVE_EXTRA_DISK_PARAM+=-s $$((${i} + 4)):0,virtio-blk,/tmp/disk${i}
CLEANFILES+=/tmp/disk${i}
.endfor
TEST_VM_NAME=	ci-${OSRELEASE}-${GITREV}-${KERNCONF}
.if ${TOTAL_MEMORY} >= 16
VM_MEM=8
.elif ${TOTAL_MEMORY} >=4
VM_MEM=${TOTAL_MEMORY}
.else
echo "Please increase the memory to at least 4GB"
exit 0
.endif
VM_MEM_SIZE?=${VM_MEM}g
TIMEOUT_MS?=5400000
TIMEOUT=$$((${TIMEOUT_MS} / 1000))
TIMEOUT_EXPECT=$$((${TIMEOUT} - 60))
TIMEOUT_VM=$$((${TIMEOUT_EXPECT} - 120))
.if exists(${.CURDIR}/Makefile.${TARGET_ARCH})
.	include "${.CURDIR}/Makefile.${TARGET_ARCH}"
.endif
.if ${TARGET_ARCH} != ${MACHINE_ARCH}
.if ( ${TARGET_ARCH} != "i386" ) || ( ${MACHINE_ARCH} != "amd64" )
QEMUSTATIC=/usr/local/bin/qemu-${QEMU_ARCH}-static
QEMUTGT=portinstall-qemu
.endif
.endif
QEMUTGT?=
QEMU_DEVICES?=-device virtio-blk,drive=hd0
QEMU_EXTRA_PARAM?=
QEMU_MACHINE?=virt
QEMUBIN=/usr/local/bin/qemu-system-${QEMU_ARCH}
.if ${PARALLEL_JOBS} >= ${QEMU_MAX_CPU_COUNT}
QEMU_CPU_COUNT=${QEMU_MAX_CPU_COUNT}
.else
QEMU_CPU_COUNT=${PARALLEL_JOBS}
.endif
.if ${VM_MEM} >= ${QEMU_MAX_MEM_SIZE}
VM_MEM_SIZE=${QEMU_MAX_MEM_SIZE}g
.else
VM_MEM_SIZE=${VM_MEM}g
.endif
VMGUEST!=sysctl -n kern.vm_guest 2>/dev/null || true
.if ${VMGUEST} != "none"
USE_QEMU?=1
.endif
KLDFILEMONISLOADED!=kldload -q -n filemon 2>/dev/null && echo "1" || echo "0"
.if ${KLDFILEMONISLOADED} == "1"
METAMODE?=-DWITH_META_MODE
.endif

CLEANFILES+=	${.OBJDIR}/${CIIMAGE} ${.OBJDIR}/ci.img ${META_TAR}
CLEANDIRS+=	${.OBJDIR}/ci-buildimage

portinstall: portinstall-pkg portinstall-qemu portinstall-expect portinstall-${TARGET_ARCH:tl} .PHONY

portinstall-pkg: .PHONY
.if !exists(/usr/local/sbin/pkg-static)
	env ASSUME_ALWAYS_YES=yes pkg bootstrap
.endif

portinstall-qemu: portinstall-pkg .PHONY
.if !exists(/usr/local/bin/qemu-${TARGET_ARCH}-static)
	env ASSUME_ALWAYS_YES=yes pkg install emulators/qemu-user-static
.endif
.if !exists(/usr/local/bin/qemu-system-${QEMU_ARCH})
	env ASSUME_ALWAYS_YES=yes pkg install emulators/qemu@nox11
.endif

portinstall-expect: portinstall-pkg .PHONY
.if !exists(/usr/local/bin/expect)
	env ASSUME_ALWAYS_YES=yes pkg install lang/expect
.endif

beforeclean: .PHONY
	chflags -R noschg ${.OBJDIR}/${.TARGET}

.include <bsd.obj.mk>
clean: beforeclean .PHONY

cleandir: beforeclean .PHONY

ci-buildworld: .PHONY
	@echo "Building world for ${TARGET_ARCH}"
	${IMAKE} -j${PARALLEL_JOBS} -C ${WORLDDIR} ${METAMODE} \
		${CROSS_TOOLCHAIN_PARAM} __MAKE_CONF=${MAKECONF} SRCCONF=${SRCCONF} \
		buildworld > ${.CURDIR}/_.${TARGET_ARCH}.${.TARGET} 2>&1 || \
		(echo "${.TARGET} failed, check _.${TARGET_ARCH}.${.TARGET} for details" ; false)


ci-buildkernel: ci-buildworld-${TARGET_ARCH:tl} .PHONY
	@echo "Building kernel for ${TARGET_ARCH"}"
	${IMAKE} -j${PARALLEL_JOBS} -C ${WORLDDIR} ${METAMODE} \
		${CROSS_TOOLCHAIN_PARAM} __MAKE_CONF=${MAKECONF} \
		SRCCONF=${SRCCONF} buildkernel > ${.CURDIR}/_.${TARGET_ARCH}.${.TARGET} 2>&1 || \
		(echo "${.TARGET} failed, check _.${TARGET_ARCH}.${.TARGET} for details" ; false)

ci-buildimage: ${QEMUTGT} ci-buildkernel-${TARGET_ARCH:tl} .PHONY
	@echo "Building ci image for ${TARGET_ARCH"}"
	mkdir -p ${.OBJDIR}/${.TARGET}
	env TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH} SWAPSIZE=${SWAPSIZE} \
		QEMUSTATIC=${QEMUSTATIC} CITYPE=${CITYPE} \
		${RELEASEDIR}/scripts/mk-vmimage.sh \
		-C ${RELEASEDIR}/tools/vmimage.subr -d ${.OBJDIR}/${.TARGET} -F ${VMFS} \
		-i ${.OBJDIR}/ci.img -s ${VMSIZE} -f ${FORMAT} \
		-S ${WORLDDIR} -o ${.OBJDIR}/${CIIMAGE} -c ${CICONF} \
		> ${.CURDIR}/_.${TARGET_ARCH}.${.TARGET} 2>&1 || \
		(echo "${.TARGET} failed, check _.${TARGET_ARCH}.${.TARGET} for details" ; false)
	touch ${.TARGET}

ci-set-smoke-var: .PHONY
CITYPE=smoke

ci-set-full-var: .PHONY
CITYPE=full

ci-create-meta: .PHONY
	truncate -s 512M ${META_TAR}
	tar rvf ${META_TAR} -C ${META_DIR} .

ci-extract-meta: .PHONY
	tar xfv ${META_TAR} -C ${META_DIROUT}
	@echo "Extracted kyua reports to ${META_DIROUT}"

ci-runtest: ci-buildimage-${TARGET_ARCH:tl} portinstall .PHONY
.if ${MACHINE} == "amd64" && ( ${TARGET_ARCH} == "amd64" || ${TARGET_ARCH} == "i386" ) && ( !defined(USE_QEMU) || empty(USE_QEMU) )
	/usr/sbin/bhyvectl --vm=${TEST_VM_NAME} --destroy || true
	/usr/sbin/bhyveload -c stdio -m ${VM_MEM_SIZE} -d ${CIDISK} ${TEST_VM_NAME}
	expect -c "set timeout ${TIMEOUT_EXPECT}; \
		spawn /usr/bin/timeout -k 60 ${TIMEOUT_VM} /usr/sbin/bhyve \
		-c ${PARALLEL_JOBS} -m ${VM_MEM_SIZE} -A -H -P \
		-s 0:0,hostbridge \
		-s 1:0,lpc \
		-s 2:0,virtio-blk,${CIDISK} \
		-s 3:0,virtio-blk,${META_TAR} \
		${BHYVE_EXTRA_DISK_PARAM} \
		-l com1,stdio \
		${TEST_VM_NAME}; \
		expect { eof }"
	/usr/sbin/bhyvectl --vm=${TEST_VM_NAME} --destroy
.else
	timeout -k 60 ${TIMEOUT_VM} ${QEMUBIN} \
		-machine ${QEMU_MACHINE} \
		-smp ${QEMU_CPU_COUNT} \
		-m ${VM_MEM_SIZE} \
		-nographic \
		-no-reboot \
		${QEMU_EXTRA_PARAM} \
		-drive if=none,file=${CIDISK},format=raw,id=hd0 \
		-drive if=none,file=${META_TAR},format=raw,id=hd1 \
		${QEMU_DEVICES}
.endif

ci-checktarget: .PHONY
.if ${TARGET_ARCH} != "aarch64" && \
	${TARGET_ARCH} != "amd64" && \
	${TARGET_ARCH} != "armv7" && \
	${TARGET_ARCH} != "powerpc64" && \
	${TARGET_ARCH} != "powerpc64le" && \
	${TARGET_ARCH} != "riscv64"
	@false
.ERROR:
	@echo "Error: ${TARGET_ARCH} is not supported on ${TYPE} ${REVISION} ${BRANCH}"
.endif

ci-smoke: ci-set-smoke-var ci-create-meta ci-checktarget .WAIT ci-runtest-${TARGET_ARCH:tl} .PHONY

ci-full: ci-set-full-var ci-create-meta ci-checktarget .WAIT ci-runtest-${TARGET_ARCH:tl} ci-extract-meta .PHONY

ci: ci-${CITYPE:tl} .PHONY

.include "${RELEASEDIR}/Makefile.inc1"
