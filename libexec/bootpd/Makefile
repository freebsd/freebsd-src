# The next few lines may be uncommented and changed to alter the default
# filenames bootpd uses for its configuration and dump files.
#CONFFILE=-DCONFIG_FILE=\"/usr/etc/bootptab\"
#DUMPFILE=-DDUMP_FILE=\"/usr/etc/bootpd.dump\"
#FILESPECS=${CONFFILE} ${DUMPFILE}

# Remove the -DVEND_CMU if you don't wish to support the "CMU vendor format"
# in addition to the RFC1048 format.

PROG=	bootpd
CFLAGS+=-DSYSLOG -DDEBUG -DVEND_CMU -DPRIVATE=static ${FILESPECS} -traditional
SRCS=	bootpd.c readfile.c hash.c version.c
MAN5=	bootptab.5
MAN8=	bootpd.8
CLEANFILES+=version.c version

version.c:
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/Version.c

.include <bsd.prog.mk>
