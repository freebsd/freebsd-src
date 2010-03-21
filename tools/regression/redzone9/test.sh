#!/bin/sh
#
# $FreeBSD: src/tools/regression/redzone9/test.sh,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

sysctl debug.redzone.malloc_underflow=1
sysctl debug.redzone.malloc_overflow=1
sysctl debug.redzone.realloc_smaller_underflow=1
sysctl debug.redzone.realloc_smaller_overflow=1
sysctl debug.redzone.realloc_bigger_underflow=1
sysctl debug.redzone.realloc_bigger_overflow=1
