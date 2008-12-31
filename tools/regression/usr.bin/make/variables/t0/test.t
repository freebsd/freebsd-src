#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/t0/test.t,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion."

eval_cmd $*
