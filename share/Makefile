#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

# Do not include `info' in the SUBDIR list, it is handled separately.

SUBDIR= colldef dict examples man me misc mk monetdef \
	msgdef numericdef skel syscons tabset termcap timedef zoneinfo

.if ${MACHINE_ARCH} != "sparc64"
SUBDIR+= mklocale
.endif

.if !defined(NO_SHAREDOCS)
SUBDIR+= doc
.endif

.if !defined(NO_I4B)
SUBDIR+= isdn
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+= sendmail
.endif

.include <bsd.subdir.mk>
