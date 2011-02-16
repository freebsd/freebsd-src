#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/modifier_M/test.t,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with M modifier"

eval_cmd $*
