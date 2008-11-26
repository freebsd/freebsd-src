#!/usr/bin/sed -E -n -f
# $FreeBSD: src/sys/conf/makeLINT.sed,v 1.3.28.1 2008/10/02 02:57:24 kensmith Exp $

/^(machine|ident|device|nodevice|makeoptions|nomakeoption|options|option|nooption|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
