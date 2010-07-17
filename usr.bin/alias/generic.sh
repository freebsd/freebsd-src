#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
