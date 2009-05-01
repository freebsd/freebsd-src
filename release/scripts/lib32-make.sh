#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . ! -path '*/libexec/*' ! -path '*/usr/lib32/*' -delete
