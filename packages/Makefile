# SPDX-License-Identifier: ISC
#
# Copyright (c) 2026 Lexi Winter <ivy@FreeBSD.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

.include <src.opts.mk>

SUBDIR=	blocklist \
	bsdconfig \
	bzip2 \
	clibs \
	cron \
	ctl \
	devd \
	devmatch \
	dhclient \
	fetch \
	flua \
	firmware-iwm \
	fwget \
	geom \
	ggate \
	lib9p \
	libarchive \
	libbegemot \
	libblocksruntime \
	libbsdstat \
	libcasper \
	libcompat \
	libcompiler_rt \
	libdwarf \
	libevent1 \
	libexecinfo \
	libpathconv \
	librss \
	libsqlite3 \
	libthread_db \
	libucl \
	libyaml \
	locales \
	mandoc \
	mtree \
	natd \
	ncurses \
	netmap \
	newsyslog \
	nfs \
	nvme-tools \
	pam \
	periodic \
	powerd \
	ppp \
	rc \
	rcmds \
	resolvconf \
	rip \
	runtime \
	smbutils \
	syslogd \
	tcpd \
	toolchain \
	ufs \
	utilities \
	xz \
	zlib \
	zstd

.if ${MK_ACCT} != "no" || ${MK_UTMPX} != "no"
SUBDIR+=	acct
.endif

.if ${MK_CLANG} != "no" || ${MK_TOOLCHAIN} != "no"
SUBDIR+=	clang
.endif

.if ${MK_AUTHPF} != "no" || ${MK_PF} != "no"
SUBDIR+=	pf
.endif

# XXX - certctl probably shouldn't depend on caroot. This logic comes from
# the src build, so we have to match it.
.if ${MK_CAROOT} != "no"
SUBDIR.${MK_OPENSSL}+=  certctl
SUBDIR+=		caroot
.endif

SUBDIR.${MK_AT}+=		at
SUBDIR.${MK_AUDIT}+=		audit
SUBDIR.${MK_AUTOFS}+=		autofs
SUBDIR.${MK_BLUETOOTH}+=	bluetooth
SUBDIR.${MK_BOOT}+=		bootloader
SUBDIR.${MK_BSDINSTALL}+=	bsdinstall
SUBDIR.${MK_BSNMP}+=		bsnmp
SUBDIR.${MK_CCD}+=		ccdconfig
SUBDIR.${MK_CUSE}+=		libcuse
SUBDIR.${MK_CXGBETOOL}+=	cxgbe-tools
SUBDIR.${MK_DMAGENT}+=		dma
SUBDIR.${MK_DTRACE}+=		ctf dtrace dwatch
SUBDIR.${MK_EE}+=		ee
SUBDIR.${MK_EFI}+=		efi-tools
SUBDIR.${MK_EXAMPLES}+=		examples
SUBDIR.${MK_FILE}+=		libmagic
SUBDIR.${MK_FLOPPY}+=		fd
SUBDIR.${MK_FTP}+=		ftp
SUBDIR.${MK_GAMES}+=		games
SUBDIR.${MK_GOOGLETEST}+=	googletest
SUBDIR.${MK_HAST}+=		hast
SUBDIR.${MK_HYPERV}+=		hyperv-tools
SUBDIR.${MK_INETD}+=		inetd
SUBDIR.${MK_IPFILTER}+=		ipf
SUBDIR.${MK_IPFW}+=		ipfw
SUBDIR.${MK_ISCSI}+=		iscsi
SUBDIR.${MK_JAIL}+=		jail
SUBDIR.${MK_KERBEROS}+=		kerberos kerberos-kdc librpcsec_gss gssd
SUBDIR.${MK_LDNS}+=		libldns
SUBDIR.${MK_LEGACY_CONSOLE}+=	console-tools
SUBDIR.${MK_LLD}+=		lld
SUBDIR.${MK_LLDB}+=		lldb
SUBDIR.${MK_LPR}+=		lp
SUBDIR.${MK_MAKE}+=		bmake
SUBDIR.${MK_MAN}+=		kernel-man
SUBDIR.${MK_MLX5TOOL}+=		mlx-tools
SUBDIR.${MK_NIS}+=		yp
SUBDIR.${MK_NTP}+=		ntp
SUBDIR.${MK_NUAGEINIT}+=	nuageinit
SUBDIR.${MK_OFED}+=		rdma
SUBDIR.${MK_OPENSSH}+=		ssh
SUBDIR.${MK_OPENSSL}+=		openssl
SUBDIR.${MK_PKGBOOTSTRAP}+=	pkg-bootstrap
SUBDIR.${MK_PKGCONF}+=		pkgconf
SUBDIR.${MK_PMC}+=		pmc
SUBDIR.${MK_QUOTAS}+=		quotacheck
SUBDIR.${MK_RESCUE}+=		rescue
SUBDIR.${MK_SENDMAIL}+=		sendmail libmilter
SUBDIR.${MK_SOUND}+=		sound
SUBDIR.${MK_SYSCONS}+=		syscons-data
SUBDIR.${MK_TCSH}+=		csh
SUBDIR.${MK_TELNET}+=		telnet
SUBDIR.${MK_TESTS}+=		tests
SUBDIR.${MK_TESTS_SUPPORT}+=	atf kyua
SUBDIR.${MK_UNBOUND}+=		local-unbound
SUBDIR.${MK_VI}+=		vi
SUBDIR.${MK_VT}+=		vt-data
SUBDIR.${MK_WIRELESS}+=		hostapd wpa
SUBDIR.${MK_ZFS}+=		zfs
SUBDIR.${MK_ZONEINFO}+=		zoneinfo

.include <bsd.arch.inc.mk>

SUBDIR_PARALLEL=

.ORDER:	all stagepackages

.include <bsd.subdir.mk>
