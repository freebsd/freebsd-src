#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . ! -path '*/libexec/*' ! -path '*/usr/lib32/*' -delete
