# $FreeBSD$

.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT)
_random=	random
.endif

SUBDIR=	3dfx \
	accf_data \
	accf_http \
	agp \
	aha \
	amr \
	an \
	aue \
	bge \
	bridge \
	cam \
	ccd \
	cd9660 \
	coda \
	cue \
	dc \
	de \
	digi \
	dummynet \
	ed \
	fdescfs \
	fdc \
	fs \
	fxp \
	gx \
	if_disc \
	if_ef \
	if_gif \
	if_faith \
	if_ppp \
	if_sl \
	if_stf \
	if_tap \
	if_tun \
	if_vlan \
	ip6fw \
	ip_mroute_mod \
	ipfilter \
	ipfw \
	ispfw \
	joy \
	kue \
	lge \
	libmchain \
	lnc \
	lomac \
	lpt \
	md \
	mii \
	mlx \
	msdosfs \
	nfsclient \
	nfsserver \
	nge \
	nmdm \
	ntfs \
	nullfs \
	pcn \
	plip \
	portalfs \
	ppbus \
	ppi \
	pps \
	procfs \
	${_random} \
	rl \
	rp \
	sf \
	sis \
	sk \
	sn \
	snp \
	sound \
	sppp \
	ste \
	sym \
	syscons \
	sysvipc \
	ti \
	tl \
	twe \
	tx \
	txp \
	udbp \
	ugen \
	uhid \
	ukbd \
	ulpt \
	umapfs \
	umass \
	umodem \
	ums \
	unionfs \
	urio \
	usb \
	uscanner \
	vinum \
	vpo \
	vr \
	vx \
	wb \
	xl

#removed while KSE settles in:
#	ncp \
#	nwfs \
# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=aac \
	acpi \
	aic \
	ar \
	apm \
	asr \
	atspeaker \
	bktr \
	coff \
	el \
	fe \
	fpu \
	gnufpu \
	ibcs2 \
	linux \
	mly \
	netgraph \
	oltr \
	pecoff \
	ray \
	s3 \
	sbni \
	splash \
	sr \
	streams \
	vesa \
	wi
.endif
#removed while KSE settles in:
#	smbfs \

.if ${MACHINE} == "pc98"
SUBDIR+=pmc \
	snc
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=linux \
	osf1
.endif

.if defined(WANT_EXT2FS_MODULE)
SUBDIR+=ext2fs
.endif

.if defined(MODULES_OVERRIDE)
SUBDIR=${MODULES_OVERRIDE}
.endif

# Calling kldxref(8) for each module is expensive.
.if !defined(NO_XREF)
.MAKEFLAGS:=	${.MAKEFLAGS} -DNO_XREF
afterinstall:
	-kldxref ${DESTDIR}${KMODDIR}
.endif

.include <bsd.subdir.mk>
