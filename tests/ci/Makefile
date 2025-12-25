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
EXTRA_MAKE_FLAGS?=

.if !defined(TARGET) || empty(TARGET)
TARGET=		${MACHINE}
.endif
.if !defined(TARGET_ARCH) || empty(TARGET_ARCH)
.  if ${TARGET} == ${MACHINE}
TARGET_ARCH=	${MACHINE_ARCH}
.  else
TARGET_ARCH=	${TARGET}
.  endif
.endif
IMAKE=		${MAKE} TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH}

.if !defined(CIENV) || empty(CIENV)
CIENV=		local
LOG_TARGET=	> ${.CURDIR}/_.${TARGET_ARCH}.${.TARGET} 2>&1 || (echo "${.TARGET} failed, check _.${TARGET_ARCH}.${.TARGET} for details" ; false)
.endif

.if defined(CROSS_TOOLCHAIN) || !empty(CROSS_TOOLCHAIN)
CROSS_TOOLCHAIN_PARAM=	"CROSS_TOOLCHAIN=${CROSS_TOOLCHAIN}"
.endif

# Define OSRELEASE by using newvers.sh
.if !defined(OSRELEASE) || empty(OSRELEASE)
.  for _V in TYPE BRANCH REVISION
.    if !defined(${_V}) || empty(${_V})
${_V}!=	eval $$(awk '/^${_V}=/{print}' ${.CURDIR}/../../sys/conf/newvers.sh); echo $$${_V}
.    endif
.  endfor
.  for _V in ${TARGET_ARCH}
.    if !empty(TARGET:M${_V})
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH:C/[.-\/]/_/g}-${TARGET}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-\/]/_/g}_${TARGET}
.    else
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH:C/[.-\/]/_/g}-${TARGET}-${TARGET_ARCH}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-\/]/_/g}_${TARGET_ARCH}
.    endif
.  endfor
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
KYUA_TEST_FILTERS?=
META_TAR!=mktemp /tmp/meta.XXXXXX
META_DIR!=mktemp -d /tmp/meta.XXXXXX
META_DIROUT!=mktemp -d /tmp/meta.XXXXXX
ENV_FILE=	${META_DIR}/ci-${OSRELEASE}-${GITREV}-${KERNCONF}.env
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
.  if ( ${TARGET_ARCH} != "i386" ) || ( ${MACHINE_ARCH} != "amd64" )
QEMUSTATIC=/usr/local/bin/qemu-${QEMU_ARCH}-static
QEMUTGT=portinstall-qemu
.  endif
.endif
QEMUTGT?=
QEMU_DEVICES?=
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

PKGBASE_REPO?=	${.OBJDIR}/pkgbase-repo
PKGBASE_REPO_DIR?= ${.OBJDIR}/pkgbase-repo-dir
WSTAGEDIR!=	${IMAKE} -f Makefile.inc1 -C ${WORLDDIR} -V WSTAGEDIR
PKG_ABI_FILE?=	${WSTAGEDIR}/usr/bin/uname
PKG_ABI?=	$$(${PKG_CMD} -o ABI_FILE=${PKG_ABI_FILE} config ABI)

CLEANFILES+=	${CIDISK} ${.OBJDIR}/ci.img ${META_TAR}
IMAGEDIR=	${.OBJDIR}/ci-buildimage
CLEANDIRS+=	${IMAGEDIR}

portinstall: portinstall-pkg portinstall-qemu portinstall-expect portinstall-${TARGET_ARCH:tl} .PHONY

portinstall-pkg: .PHONY
.if !exists(/usr/local/sbin/pkg-static)
	env ASSUME_ALWAYS_YES=yes pkg bootstrap
.endif

portinstall-qemu: portinstall-pkg .PHONY
.if !empty(QEMUSTATIC) && !exists(${QEMUSTATIC})
	env ASSUME_ALWAYS_YES=yes pkg install emulators/qemu-user-static
.endif
.if !exists(${QEMUBIN})
	env ASSUME_ALWAYS_YES=yes pkg install emulators/qemu@nox11
.endif

portinstall-expect: portinstall-pkg .PHONY
.if !exists(/usr/local/bin/expect)
	env ASSUME_ALWAYS_YES=yes pkg install lang/expect
.endif

beforeclean: .PHONY
	chflags -R noschg ${IMAGEDIR}

.include <bsd.obj.mk>
clean: beforeclean .PHONY

cleandir: beforeclean .PHONY

ci-buildpkgbase: .PHONY
	@echo "Building world, kernel, and pkgbase for ${TARGET_ARCH}"
	${IMAKE} -j${PARALLEL_JOBS} -C ${WORLDDIR} ${METAMODE} \
		${CROSS_TOOLCHAIN_PARAM} __MAKE_CONF=${MAKECONF} SRCCONF=${SRCCONF} \
		${EXTRA_MAKE_FLAGS} KERNCONF=${KERNCONF} REPODIR=${PKGBASE_REPO} \
		buildworld buildkernel update-packages ${LOG_TARGET}
	mkdir -p ${PKGBASE_REPO_DIR}
	@printf "FreeBSD-base: { url: "file://%s", enabled: yes }" \
		${PKGBASE_REPO}/${PKG_ABI}/latest \
		> ${PKGBASE_REPO_DIR}/FreeBSD-base.conf

ci-buildimage: ${QEMUTGT} ci-buildpkgbase .PHONY
	@echo "Building ci image for ${TARGET_ARCH}"
	mkdir -p ${.OBJDIR}/${.TARGET}
	env TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH} SWAPSIZE=${SWAPSIZE} \
		QEMUSTATIC=${QEMUSTATIC} CITYPE=${CITYPE} \
		PKG_CMD=${PKG_CMD:Upkg} \
		PKG_ABI=${PKG_ABI} \
		PKGBASE_REPO_DIR=${PKGBASE_REPO_DIR} \
		${RELEASEDIR}/scripts/mk-vmimage.sh \
		-C ${RELEASEDIR}/tools/vmimage.subr -d ${.OBJDIR}/${.TARGET} -F ${VMFS} \
		-i ${.OBJDIR}/ci.img -s ${VMSIZE} -f ${FORMAT} \
		-S ${WORLDDIR} -o ${CIDISK} -c ${CICONF} ${LOG_TARGET}
	touch ${.TARGET}

ci-get-env: .PHONY
	@echo "TARGET=${TARGET}" > ${ENV_FILE}
	@echo "TARGET_ARCH=${TARGET_ARCH}" >> ${ENV_FILE}
	@echo "KERNCONF=${KERNCONF}" >> ${ENV_FILE}
	@echo "OSRELEASE=${OSRELEASE}" >> ${ENV_FILE}
	@echo "CIIMAGE=${CIIMAGE}" >> ${ENV_FILE}
	@echo "CIDISK=${CIDISK}" >> ${ENV_FILE}
	@echo "VMSIZE=${VMSIZE}" >> ${ENV_FILE}
	@echo "VM_MEM_SIZE=${VM_MEM_SIZE}" >> ${ENV_FILE}
	@echo "TIMEOUT=${TIMEOUT}s" >> ${ENV_FILE}
	@echo "CITYPE=${CITYPE}" >> ${ENV_FILE}
	@echo "KYUA_TEST_FILTERS='${KYUA_TEST_FILTERS}'" >> ${ENV_FILE}
	@echo "META_MODE='${METAMODE}'" >> ${ENV_FILE}
	@echo "USE_QEMU='${USE_QEMU}'" >> ${ENV_FILE}
	@echo "GITREV='${GITREV}'" >> ${ENV_FILE}
	@echo "GITBRANCH='${GITBRANCH}'" >> ${ENV_FILE}
	@echo "GITCOUNT='${GITCOUNT}'" >> ${ENV_FILE}
	@echo "BUILDDATE='${BUILDDATE}'" >> ${ENV_FILE}
	@echo "CC='${CC}'" >> ${ENV_FILE}
	@echo "CFLAGS='${CFLAGS}'" >> ${ENV_FILE}
	@echo "CPP='${CPP}'" >> ${ENV_FILE}
	@echo "CXX='${CXX}'" >> ${ENV_FILE}
	@echo "CXXFLAGS='${CXXFLAGS}'" >> ${ENV_FILE}
	@echo "COMPILER_FEATURES='${COMPILER_FEATURES}'" >> ${ENV_FILE}
	@echo "COMPILER_FREEBSD_VERSION='${COMPILER_FREEBSD_VERSION}'" >> ${ENV_FILE}
	@echo "COMPILER_TYPE='${COMPILER_TYPE}'" >> ${ENV_FILE}
	@echo "COMPILER_VERSION='${COMPILER_VERSION}'" >> ${ENV_FILE}
	@echo "HOST_CC='${HOST_CC}'" >> ${ENV_FILE}
	@echo "HOST_MACHINE='${HOST_MACHINE}'" >> ${ENV_FILE}
	@echo "HOST_OS='${HOST_OS}'" >> ${ENV_FILE}
	@echo "HOST_OSMAJOR='${HOST_OSMAJOR}'" >> ${ENV_FILE}
	@echo "HOST_OSTYPE='${HOST_OSTYPE}'" >> ${ENV_FILE}
	@echo "LD='${LD}'" >> ${ENV_FILE}
	@echo "LDFLAGS='${LDFLAGS}'" >> ${ENV_FILE}
	@echo "MACHINE='${MACHINE}'" >> ${ENV_FILE}
	@echo "MACHINE_ABI='${MACHINE_ABI}'" >> ${ENV_FILE}
	@echo "MACHINE_ARCH='${MACHINE_ARCH}'" >> ${ENV_FILE}
	@echo "MACHINE_CPU='${MACHINE_CPU}'" >> ${ENV_FILE}
	@echo "MACHINE_CPUARCH='${MACHINE_CPUARCH}'" >> ${ENV_FILE}

ci-create-meta: ci-get-env .PHONY
	truncate -s 512M ${META_TAR}
	tar rvf ${META_TAR} -C ${META_DIR} .

ci-extract-meta: .PHONY
	tar xfv ${META_TAR} -C ${META_DIROUT}
	rm -rf ${META_TAR} ${META_DIR}
	chmod 0755 ${META_DIROUT}
	@echo "Extracted kyua reports to ${META_DIROUT}"

ci-runtest: ci-create-meta portinstall .WAIT ci-runtest-${TARGET_ARCH:tl} .WAIT ci-extract-meta .PHONY

ci-runtest-qemu: .PHONY
	timeout -k 60 ${TIMEOUT_VM} ${QEMUBIN} \
		-machine ${QEMU_MACHINE} \
		-smp ${QEMU_CPU_COUNT} \
		-m ${VM_MEM_SIZE} \
		-nographic \
		-no-reboot \
		${QEMU_EXTRA_PARAM} \
		-device virtio-blk,drive=hd0 \
		-device virtio-blk,drive=hd1 \
		-blockdev driver=raw,node-name=hd0,file.driver=file,file.filename=${CIDISK} \
		-blockdev driver=raw,node-name=hd1,file.driver=file,file.filename=${META_TAR} \
		${QEMU_DEVICES}

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

ci: ci-buildimage-${TARGET_ARCH:tl} .WAIT ci-runtest .PHONY

.include "${RELEASEDIR}/Makefile.inc1"
