#	@(#)Makefile	8.1 (Berkeley) 6/5/93

SUBDIR= dict doc examples info man me misc mk skel \
	tabset termcap timedef zoneinfo

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= syscons
.endif

.include <bsd.subdir.mk>
