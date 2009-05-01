#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_cc/RunTest.sh,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_cc

$LOCALBASE/bin/ats_cc $options $DATA/CC_Funcs $DATA/CC_??_??
