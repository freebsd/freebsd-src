#!/bin/sh
#
# $FreeBSD: src/tools/regression/redzone9/test.sh,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

sysctl debug.redzone.malloc_underflow=1
sysctl debug.redzone.malloc_overflow=1
sysctl debug.redzone.realloc_smaller_underflow=1
sysctl debug.redzone.realloc_smaller_overflow=1
sysctl debug.redzone.realloc_bigger_underflow=1
sysctl debug.redzone.realloc_bigger_overflow=1
