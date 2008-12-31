#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_uni/RunTest.sh,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_uni

$LOCALBASE/bin/ats_sig $options $DATA/Funcs $DATA/L3MU_Funcs $DATA/Restart.??? \
	$DATA/Unknown.??? $DATA/Incoming.??? $DATA/MpOut.??? $DATA/MpIn.??? \
	$DATA/L???_??_??
