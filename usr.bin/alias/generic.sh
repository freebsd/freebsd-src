#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
