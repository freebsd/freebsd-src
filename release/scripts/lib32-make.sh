#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.22.2.2.1 2009/10/25 01:10:29 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
