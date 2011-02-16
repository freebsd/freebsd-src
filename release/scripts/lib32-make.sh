#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.10.1.4.1 2010/12/21 17:10:29 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
