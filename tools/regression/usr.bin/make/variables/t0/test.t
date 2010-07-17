#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/t0/test.t,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion."

eval_cmd $*
