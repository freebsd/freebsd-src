# $FreeBSD$

# XXX present but broken: ip_mroute_mod

.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT)
_random=	random
.endif

SUBDIR=	3dfx accf_data accf_http agp aha amr an aue \
	cam ccd cd9660 coda cue dc de dgm digi ed fdescfs fs fxp if_disc if_ef \
	if_ppp if_sl if_tap if_tun ip6fw ipfilter ipfw ispfw joy kue lge \
	libmchain linux lnc md mii mlx msdosfs ncp netgraph nfs nge ntfs \
	nullfs nwfs pcn portalfs procfs ${_random}  \
	rl rp sf sis sk sn snp sound sppp ste sym syscons sysvipc ti tl twe \
	tx udbp ugen uhid ukbd ulpt umapfs umass umodem ums unionfs urio usb \
	uscanner \
	vinum vpo vr vx wb wx xl

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=aac aic ar asr atspeaker bktr coff el fpu gnufpu ibcs2 linprocfs mly \
	pecoff ray s3 smbfs splash sr streams vesa wi
.endif

.if ${MACHINE} == "pc98"
SUBDIR+=snc
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=osf1
.endif

.if defined(MODULES_OVERRIDE)
SUBDIR=${MODULES_OVERRIDE}
.endif

.include <bsd.subdir.mk>
