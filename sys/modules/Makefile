# $FreeBSD: src/sys/modules/Makefile,v 1.110 2000/03/01 07:42:09 sheldonh Exp $

# XXX present but broken: ip_mroute_mod pcic

SUBDIR=	aha amr an aue ccd cd9660 coda cue dc fdesc fxp if_disc if_ef if_ppp \
	if_sl if_tun ipfilter ipfw joy kernfs kue md mfs mii mlx msdos \
	ncp netgraph nfs ntfs nullfs \
	nwfs portal procfs rl sf sis sk ste ti tl \
	ugen uhid ukbd ulpt umapfs umass umodem ums union usb vn vpo vr wb xl

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=bktr coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa vinum wi
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=osf1
.endif

.include <bsd.subdir.mk>
