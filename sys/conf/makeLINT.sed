#!/usr/bin/sed -E -n -f
# $FreeBSD: src/sys/conf/makeLINT.sed,v 1.3.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

/^(machine|ident|device|nodevice|makeoptions|nomakeoption|options|option|nooption|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
