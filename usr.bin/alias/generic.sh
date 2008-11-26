#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.1.16.1.8.1 2008/10/02 02:57:24 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
