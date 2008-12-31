#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/modifier_M/test.t,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with M modifier"

eval_cmd $*
