# $FreeBSD$

# XXX present but broken: atapi ip_mroute_mod joy pcic

SUBDIR=	aha al ax ccd cd9660 coda dm fdesc fxp if_disc if_ppp if_sl if_tun \
	ipfw kernfs md mfs mii msdos mx nfs ntfs nullfs pn portal procfs rl \
	sf sis sk ste ti tl umapfs union vn vr wb xl

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
# XXX broken bktr
SUBDIR+=coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa vinum
.endif

.include <bsd.subdir.mk>
