#	$Id: Makefile,v 1.63 1999/06/02 07:15:17 jb Exp $

# XXX present but broken: atapi ip_mroute_mod joy pcic
# XXX vinum removed pending cdevsw changes review by grog.

SUBDIR=	ccd cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=bktr coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa
.endif

.include <bsd.subdir.mk>
