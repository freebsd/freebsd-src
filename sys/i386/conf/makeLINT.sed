#!/usr/bin/sed -E -n -f
# $FreeBSD$

/^(machine|ident|device|makeoptions|options|profile|cpu|option|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
