#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_cc/RunTest.sh,v 1.1.24.1 2008/10/02 02:57:24 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_cc

$LOCALBASE/bin/ats_cc $options $DATA/CC_Funcs $DATA/CC_??_??
