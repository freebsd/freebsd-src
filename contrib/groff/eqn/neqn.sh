#!/bin/sh
# Provision of this shell script should not be taken to imply that use of
# GNU eqn with groff -Tascii|-Tlatin1 is supported.

# Default device.
if test `expr "$LC_CTYPE" : ".*\.ISO_8859-1"` -gt 0 || \
   test `expr "$LANG" : ".*\.ISO_8859-1"` -gt 0
then
	T=-Tlatin1
else
if test `expr "$LC_CTYPE" : ".*\.KOI8-R"` -gt 0 || \
   test `expr "$LANG" : ".*\.KOI8-R"` -gt 0
then
	T=-Tkoi8-r
else
	T=-Tascii
fi
fi

exec @g@eqn -T${T} ${1+"$@"}
