#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
