#!/usr/bin/sed -E -n -f
# $FreeBSD: src/sys/conf/makeLINT.sed,v 1.3.32.1 2009/04/15 03:14:26 kensmith Exp $

/^(machine|ident|device|nodevice|makeoptions|nomakeoption|options|option|nooption|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
