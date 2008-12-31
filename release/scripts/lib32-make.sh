#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . ! -path '*/libexec/*' ! -path '*/usr/lib32/*' -delete
