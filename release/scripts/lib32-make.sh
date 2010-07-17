#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.22.2.4.1 2010/06/14 02:09:06 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
