#!/usr/bin/sed -E -n -f
# $FreeBSD: src/sys/conf/makeLINT.sed,v 1.1 2002/05/02 16:34:47 des Exp $

/^(machine|ident|device|makeoptions|options|profile|cpu|option|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
