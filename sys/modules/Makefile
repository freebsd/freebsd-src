# $FreeBSD$

# XXX present but broken: ip_mroute_mod pcic

SUBDIR=	aha amr an aue ccd cd9660 coda cue dc fdesc fxp if_disc if_ef if_ppp \
	if_sl if_tun ipfilter ipfw ispfw joy kernfs kue \
	md mfs mii mlx msdos ncp netgraph nfs ntfs nullfs \
	nwfs portal procfs rl sf sis sk ste syscons ti tl twe \
	ugen uhid ukbd ulpt umapfs umass umodem ums union usb vn vpo vr wb wx xl

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=bktr coff fpu gnufpu ibcs2 linprocfs linux splash streams \
	svr4 vesa vinum wi
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=osf1
.endif

.include <bsd.subdir.mk>
