.include <src.opts.mk>

MANGROUPS=	MAN

MAN=	\
	beinstall.8 \
	crash.8 \
	debug.sh.8 \
	diskless.8 \
	intro.8 \
	nanobsd.8 \
	rescue.8 \
	${_uefi.8}

MLINKS= \
	beinstall.8 beinstall.sh.8 \
	nanobsd.8 nanobsd.sh.8

MANGROUPS+=	RC
RC=		rc.8 rc.subr.8
RCLINKS=	rc.8 rc.d.8 \
		rc.8 rc.firewall.8 \
		rc.8 rc.local.8 \
		rc.8 rc.network.8 \
		rc.8 rc.pccard.8 \
		rc.8 rc.resume.8 \
		rc.8 rc.serial.8 \
		rc.8 rc.shutdown.8
RCPACKAGE=	rc

.if ${MK_NIS} != "no"
MANGROUPS+=	YP
YP=		yp.8
YPLINKS=	yp.8 NIS.8 \
		yp.8 nis.8 \
		yp.8 YP.8
YPPACKAGE=	yp
.endif

# This makes more sense for aarch 64 and amd64
# we decide to install all manpages in all architectures
_uefi.8= uefi.8

MLINKS+=uefi.8 efi.8

.include <bsd.prog.mk>
