#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-make.sh,v 1.1 2005/06/16 18:16:13 ru Exp $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . ! -path '*/libexec/*' ! -path '*/usr/lib32/*' -delete
