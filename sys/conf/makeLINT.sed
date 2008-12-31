#!/usr/bin/sed -E -n -f
# $FreeBSD: src/sys/conf/makeLINT.sed,v 1.3.30.1 2008/11/25 02:59:29 kensmith Exp $

/^(machine|ident|device|nodevice|makeoptions|nomakeoption|options|option|nooption|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
