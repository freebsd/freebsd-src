# $FreeBSD$

# XXX present but broken: ip_mroute_mod pcic

SUBDIR=	accf_data \
	accf_http \
	agp \
	aha \
	amr \
	an \
	aue \
	bge \
	bridge \
	ccd \
	cd9660 \
	coda \
	cue \
	dc \
	dummynet \
	fdesc \
	fxp \
	gx \
	if_disc \
	if_ef \
	if_faith \
	if_gif \
	if_ppp \
	if_sl \
	if_stf \
	if_tap \
	if_tun \
	if_vlan \
	ip6fw \
	ipfilter \
	ipfw \
	ispfw \
	joy \
	kernfs \
	kue \
	lge \
	libiconv \
	libmchain \
	linux \
	md \
	mfs \
	mii \
	mlx \
	msdos \
	ncp \
	pcn \
	netgraph \
	nfs \
	nge \
	nmdm \
	ntfs \
	nullfs \
	nwfs \
	portal \
	procfs \
	rl \
	sf \
	sis \
	sk \
	snp \
	sound \
	ste \
	syscons \
	ti \
	tl \
	twe \
	txp \
	ufm \
	ugen \
	uhid \
	ukbd \
	ulpt \
	umapfs \
	umass \
	umodem \
	ums \
	union \
	usb \
	uscanner \
	vinum \
	vn \
	vpo \
	vr \
	wb \
	wx \
	xl

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=aac \
	asr \
	bktr \
	ciss \
	coff \
	em \
	fpu \
	gnufpu \
	ibcs2 \
	linprocfs \
	mly \
	ncv \
	nsp \
	ray \
	sbni \
	scsi_low \
	smbfs \
	splash \
	stg \
	streams \
	svr4 \
	vesa \
	wi \
	xe
.endif

.if ${MACHINE} == "pc98"
SUBDIR+=pmc \
	snc
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=osf1
.endif

.if defined(MODULES_OVERRIDE)
SUBDIR=${MODULES_OVERRIDE}
.endif

.include <bsd.subdir.mk>
