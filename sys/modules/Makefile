#	$Id: Makefile,v 1.62 1999/05/15 06:13:27 grog Exp $

# XXX present but broken: atapi ip_mroute_mod joy pcic
# XXX vinum removed pending cdevsw changes review by grog.

SUBDIR=	ccd cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa
.endif

.include <bsd.subdir.mk>
