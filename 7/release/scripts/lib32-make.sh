#!/bin/sh
#
# $FreeBSD$
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . ! -path '*/libexec/*' ! -path '*/usr/lib32/*' -delete
