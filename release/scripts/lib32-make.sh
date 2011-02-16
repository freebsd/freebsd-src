#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.22.2.6.1 2010/12/21 17:09:25 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
