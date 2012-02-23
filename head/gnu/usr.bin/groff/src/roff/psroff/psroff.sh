#! /bin/sh -
#
# $FreeBSD$

exec groff -Tps -l -C ${1+"$@"}
