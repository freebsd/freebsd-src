#!/usr/bin/sed -E -n -f
# $FreeBSD$

/^(machine|ident|device|nodevice|makeoptions|nomakeoption|options|option|nooption|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
