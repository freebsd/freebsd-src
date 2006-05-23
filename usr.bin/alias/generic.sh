#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.1.10.1 2005/11/04 18:22:17 cperciva Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
