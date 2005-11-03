#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/t0/test.t,v 1.2 2005/05/31 14:13:07 harti Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion."

eval_cmd $*
