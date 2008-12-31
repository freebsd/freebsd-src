#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
