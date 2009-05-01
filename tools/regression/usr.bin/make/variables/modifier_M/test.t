#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/modifier_M/test.t,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with M modifier"

eval_cmd $*
