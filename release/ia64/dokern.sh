#!/bin/sh
#
# $FreeBSD: src/release/ia64/dokern.sh,v 1.1 2002/11/02 20:31:54 marcel Exp $
#

sed	-e 's/ident.*GENERIC/ident		BOOTMFS/g'
