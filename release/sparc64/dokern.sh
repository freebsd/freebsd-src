#!/bin/sh
#
# $FreeBSD: src/release/sparc64/dokern.sh,v 1.1 2002/10/13 18:36:06 jake Exp $
#

sed	-e 's/ident.*GENERIC/ident		BOOTMFS/g'
