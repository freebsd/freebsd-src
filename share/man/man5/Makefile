.include <src.opts.mk>

MANGROUPS=	MAN

#MISSING: dump.5 plot.5
MAN=	a.out.5 \
	${_boot.config.5} \
	core.5 \
	dir.5 \
	disktab.5 \
	elf.5 \
	ethers.5 \
	eui64.5 \
	fbtab.5 \
	forward.5 \
	group.5 \
	hosts.5 \
	hosts.equiv.5 \
	intro.5 \
	libmap.conf.5 \
	link.5 \
	mailer.conf.5 \
	make.conf.5 \
	motd.5 \
	mount.conf.5 \
	networks.5 \
	nsswitch.conf.5 \
	os-release.5 \
	pbm.5 \
	phones.5 \
	portindex.5 \
	protocols.5 \
	quota.user.5 \
	rctl.conf.5 \
	regdomain.5 \
	remote.5 \
	resolver.5 \
	services.5 \
	shells.5 \
	src.conf.5 \
	stab.5 \
	style.Makefile.5 \
	style.mdoc.5 \
	sysctl.conf.5 \

MLINKS=	dir.5 dirent.5
MLINKS+=fs.5 inode.5
MLINKS+=hosts.equiv.5 rhosts.5
MLINKS+=portindex.5 INDEX.5
MLINKS+=quota.user.5 quota.group.5
MLINKS+=resolver.5 resolv.conf.5
MLINKS+=src.conf.5 src-env.conf.5

MANGROUPS+=	ACCT
ACCT=		acct.5
ACCTPACKAGE=	acct

MANGROUPS.${MK_BLUETOOTH}+=		BLUETOOTH
BLUETOOTHPACKAGE=			bluetooth
BLUETOOTH=	bluetooth.device.conf.5 \
		bluetooth.hosts.5 \
		bluetooth.protocols.5

MANGROUPS+=				BOOTLOADER
BOOTLOADERPACKAGE=			bootloader
BOOTLOADER=	device.hints.5

MANGROUPS.${MK_FREEBSD_UPDATE}+=	FREEBSD_UPDATE
FREEBSD_UPDATE=	freebsd-update.conf.5

MANGROUPS.${MK_HESIOD}+=		HESIOD
HESIOD=		hesiod.conf.5

MANGROUPS.${MK_TOOLCHAIN}+=		TOOLCHAIN
TOOLCHAINPACKAGE=			toolchain
TOOLCHAIN=	ar.5

MANGROUPS+=				LP
LPPACKAGE=				lp
LP=		hosts.lpd.5

MANGROUPS+=				PERIODIC
PERIODICPACKAGE=			periodic
PERIODIC=	periodic.conf.5

MANGROUPS.${MK_PF}+=			PF
PFPACKAGE=				pf
PF=		pf.conf.5 \
		pf.os.5

MANGROUPS+=				RC
RCPACKAGE=				rc
RC=		rc.conf.5
RCLINKS=	rc.conf.5 rc.conf.local.5

MANGROUPS+=				RUNTIME
RUNTIMEPACKAGE=				runtime
RUNTIME=	devfs.conf.5 \
		devfs.rules.5 \
		fstab.5 \
		passwd.5
RUNTIMELINKS=	passwd.5 master.passwd.5
RUNTIMELINKS+=	passwd.5 pwd.db.5
RUNTIMELINKS+=	passwd.5 spwd.db.5

MANGROUPS+=				SMB
SMBPACKAGE=				smbutils
SMB=		nsmb.conf.5

MANGROUPS.${MK_OPENSSH}+=		SSH
SSHPACKAGE=				ssh
SSH=		moduli.5

MANGROUPS+=				UFS
UFSPACKAGE=				ufs
UFS=		fs.5

# This makes more sense for amd64 and i386 but
# we decide to install all manpages in all architectures
_boot.config.5=	boot.config.5

.include <bsd.prog.mk>
