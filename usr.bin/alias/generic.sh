#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
