#	$NetBSD: mkconf,v 1.1.1.1 1997/07/24 21:20:12 christos Exp $
# $FreeBSD: src/usr.sbin/amd/include/newvers.sh,v 1.3 1999/08/28 01:15:16 peter Exp $
# mkconf
# Generate local configuration parameters for amd
#
cat << __EOF

/* Define name of host machine's cpu (eg. sparc) */
/* #define HOST_CPU "`uname -p`" */
#define HOST_CPU "`uname -m`"

/* Define name of host machine's architecture (eg. sun4) */
#define HOST_ARCH "`uname -m`"

/* Define name and version of host machine (eg. solaris2.5.1) */
#define HOST_OS "`uname -s | tr '[A-Z]' '[a-z]'``uname -r`"

/* Define only name of host machine OS (eg. solaris2) */
#define HOST_OS_NAME "`uname -s | tr '[A-Z]' '[a-z]'``uname -r | sed -e 's/\..*$//'`"

/* Define only version of host machine (eg. 2.5.1) */
#define HOST_OS_VERSION "`uname -r`"

/* Define name of host */
#define HOST_NAME "`hostname`"

/* Define user name */
#define USER_NAME "`whoami`"

/* Define configuration date */
#define CONFIG_DATE "`date`"

__EOF
