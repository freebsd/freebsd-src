.if exists(${.CURDIR}/../libunix/obj)
LIBUNIX=	$(.CURDIR)/../libunix/obj/libunix.a
.else
LIBUNIX=	$(.CURDIR)/../libunix/libunix.a
.endif

.if exists(${.CURDIR}/../libuuconf/obj)
LIBUUCONF=	$(.CURDIR)/../libuuconf/obj/libuuconf.a
.else
LIBUUCONF=	$(.CURDIR)/../libuuconf/libuuconf.a
.endif

.if exists(${.CURDIR}/../libuucp/obj)
LIBUUCP=	$(.CURDIR)/../libuucp/obj/libuucp.a
.else
LIBUUCP=	$(.CURDIR)/../libuucp/libuucp.a
.endif

VERSION=        1.05
owner=		uucp
group=		wheel
bindir=		/usr/bin
sbindir=	/usr/libexec/uucp

# The directory to look in for new style configuration files (when
# using HAVE_TAYLOR_CONFIG).
newconfigdir=	/etc/uucp

# The directory to look in for BNU (when using HAVE_BNU_CONFIG) or
# V2 (when using HAVE_V2_CONFIG) style configuration files.
oldconfigdir=	/etc/uucp
