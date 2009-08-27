#!/bin/sh
#
# $FreeBSD$
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . '(' -path '*/usr/share/*' -or -path '*/usr/lib/*' ')' -delete
