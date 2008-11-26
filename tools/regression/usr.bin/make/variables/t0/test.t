#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/t0/test.t,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion."

eval_cmd $*
