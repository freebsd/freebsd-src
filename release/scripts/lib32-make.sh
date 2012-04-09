#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.22.2.8.1 2012/03/03 06:15:13 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
