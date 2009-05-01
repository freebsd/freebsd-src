#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/t0/test.t,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion."

eval_cmd $*
