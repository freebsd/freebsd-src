# $FreeBSD$

# XXX present but broken: atapi ip_mroute_mod joy pcic

SUBDIR=	aha ccd dc cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun \
	ipfilter ipfw kernfs md mfs mii msdos netgraph nfs ntfs nullfs \
	portal procfs rl sf sis sk ste ti tl umapfs union vn vr wb xl

SUBDIR+=usb ugen uhid ukbd ulpt ums umodem umass

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=amr bktr coff fpu gnufpu ibcs2 linux mlx ncp nwfs splash streams \
	svr4 syscons vesa vinum wi
.endif

.include <bsd.subdir.mk>
