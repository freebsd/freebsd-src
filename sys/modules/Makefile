# $FreeBSD$

# XXX present but broken: ip_mroute_mod pcic

SUBDIR=	accf_data \
	accf_http \
	agp \
	aha \
	amr \
	an \
	aue \
	axe \
	bfe \
	bge \
	bridge \
	cam \
	ccd \
	cd9660 \
	coda \
	crypto \
	cryptodev \
	cue \
	dc \
	dcons \
	dcons_crom \
	dummynet \
	ed \
	em \
	ext2fs \
	fdesc \
	firewire \
	fxp \
	gx \
	ida \
	if_disc \
	if_ef \
	if_faith \
	if_gif \
	if_gre \
	if_ppp \
	if_sl \
	if_stf \
	if_tap \
	if_tun \
	if_vlan \
	iir \
	ip6fw \
	ipfw \
	ips \
	ispfw \
	joy \
	kue \
	lge \
	libiconv \
	libmchain \
	linux \
	md \
	mfi \
	mfs \
	mii \
	mlx \
	msdos \
	my \
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
	rp \
	rue \
	sbsh \
	sf \
	sis \
	sk \
	snp \
	sound \
	ste \
	syscons \
	ti \
	tl \
	trm \
	twe \
	txp \
	ubsa \
	ucom \
	ufm \
	uftdi \
	ugen \
	uhid \
	ukbd \
	ulpt \
	umapfs \
	umass \
	umct \
	umodem \
	ums \
	union \
	uplcom \
	usb \
	uscanner \
	uvisor \
	uvscom \
	vinum \
	vn \
	vpo \
	vr \
	wb \
	wx \
	xl

.if !defined(NO_IPFILTER)
SUBDIR+=ipfilter
.endif

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=ar \
	bktr \
	coff \
	fpu \
	gnufpu \
	hifn \
	ibcs2 \
	linprocfs \
	ncv \
	nsp \
	ray \
	safe \
	sbni \
	scsi_low \
	smbfs \
	splash \
	sppp \
	sr \
	stg \
	streams \
	svr4 \
	ubsec \
	wi \
	xe

.if ${MACHINE} == "i386"
SUBDIR+=aac \
	arl \
	asr \
	ciss \
	mly \
	pst \
	twa \
	vesa

.elif ${MACHINE} == "pc98"
SUBDIR+=canbepm \
	canbus \
	pmc \
	snc
.endif
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=osf1
.endif

.if defined(MODULES_OVERRIDE)
SUBDIR=${MODULES_OVERRIDE}
.endif

.include <bsd.subdir.mk>
