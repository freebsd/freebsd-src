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
	sound \
	ste \
	syscons \
	ti \
	tl \
	twe \
	txp \
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
	fpu \
	gnufpu \
	ibcs2 \
	linprocfs \
	mly \
	ray \
	smbfs \
	splash \
	streams \
	svr4 \
	vesa \
	wi
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
