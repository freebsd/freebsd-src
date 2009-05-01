#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
