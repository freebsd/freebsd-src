#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/modifier_M/test.t,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with M modifier"

eval_cmd $*
