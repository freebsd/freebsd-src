#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_cc/RunTest.sh,v 1.1.32.1 2010/02/10 00:26:20 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_cc

$LOCALBASE/bin/ats_cc $options $DATA/CC_Funcs $DATA/CC_??_??
