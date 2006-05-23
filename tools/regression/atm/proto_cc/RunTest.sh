#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_cc/RunTest.sh,v 1.1 2004/08/13 09:27:21 harti Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_cc

$LOCALBASE/bin/ats_cc $options $DATA/CC_Funcs $DATA/CC_??_??
