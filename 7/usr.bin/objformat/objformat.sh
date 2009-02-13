#! /bin/sh
# $FreeBSD$
# /usr/bin/objformat has been obsolete and deprecated for years.
# Please remove any build/configure script references.  New software
# should only have to only support elf on FreeBSD.
#
# FreeBSD-2.0, 2.1.x and 2.2.x will use a.out
# FreeBSD-3.x will have a real /usr/bin/objformat and are more likely
#  to be elf than a.out.
# Assume that FreeBSD-4.x will be using elf even though it is
#  **theoretically** possible to build an a.out world.
# FreeBSD-5.x and higher only support elf.
#

echo '========================================================' 1>&2
echo '== PLEASE REMOVE ALL REFERENCES TO /usr/bin/objformat ==' 1>&2
echo '=========== IT HAS BEEN OBSOLETE FOR YEARS! ====-=======' 1>&2
echo '========================================================' 1>&2
(echo '========================================================' >/dev/tty) 2>/dev/null
(echo '== PLEASE REMOVE ALL REFERENCES TO /usr/bin/objformat ==' >/dev/tty) 2>/dev/null
(echo '=========== IT HAS BEEN OBSOLETE FOR YEARS! ====-=======' >/dev/tty) 2>/dev/null
(echo '========================================================' >/dev/tty) 2>/dev/null
# highlight the nag or it will never be fixed!
sleep 10
echo elf
exit 0
