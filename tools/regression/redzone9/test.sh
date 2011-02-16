#!/bin/sh
#
# $FreeBSD: src/tools/regression/redzone9/test.sh,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

sysctl debug.redzone.malloc_underflow=1
sysctl debug.redzone.malloc_overflow=1
sysctl debug.redzone.realloc_smaller_underflow=1
sysctl debug.redzone.realloc_smaller_overflow=1
sysctl debug.redzone.realloc_bigger_underflow=1
sysctl debug.redzone.realloc_bigger_overflow=1
