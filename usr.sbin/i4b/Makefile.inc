# $FreeBSD$

# if you don't like curses stuff in the daemon (i.e. don't intend
# to ever run it in the foreground but are using the monitoring
# utilities instead) define this to compile it without.
#I4B_WITHOUT_CURSES	=	1

# if you would like monitoring support, define this
I4B_EXTERNAL_MONITOR	=	1

# for the security conscious type: restrict monitoring to the
# local machine by not compiling any tcp/ip support for monitoring
# at all
#I4B_NOTCPIP_MONITOR	=	1

.include "${.CURDIR}/../../Makefile.inc"
