#!/bin/sh
# Provision of this shell script should not be taken to imply that use of
# GNU eqn with groff -Tascii|-Tlatin1|-Tutf8|-Tcp1047 is supported.
# $FreeBSD$

# Default device.
locale=${LC_CTYPE:-$LANG}
if test `expr "$locale" : ".*\.ISO_8859-1"` -gt 0
then
	T=latin1
else
if test `expr "$locale" : ".*\.KOI8-R"` -gt 0
then
	T=koi8-r
else
	T=ascii
fi
fi

exec @g@eqn -T${T} ${1+"$@"}
