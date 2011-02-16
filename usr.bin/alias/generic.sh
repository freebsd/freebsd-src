#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
