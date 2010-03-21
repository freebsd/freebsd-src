#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.10.1.2.1 2010/02/10 00:26:20 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
