#!/bin/bash

LOGDIR=$1
POSTFIX=$2
RESTORE=$3

DIR=$PWD
TMPDIR=/tmp/logs

mv $LOGDIR/alt-wpa_supplicant $TMPDIR
mv $LOGDIR/alt-hostapd $TMPDIR
mv $LOGDIR/alt-hostapd-as $TMPDIR
mv $LOGDIR/alt-hlr_auc_gw $TMPDIR

cd $TMPDIR/alt-wpa_supplicant/wpa_supplicant
lcov -c -d .. 2> lcov.log | sed s%SF:/tmp/logs/alt-[^/]*/%SF:/tmp/logs/alt-wpa_supplicant/% > $TMPDIR/lcov-wpa_supplicant.info-$POSTFIX &

cd $TMPDIR/alt-hostapd/hostapd
lcov -c -d .. 2> lcov.log | sed s%SF:/tmp/logs/alt-[^/]*/%SF:/tmp/logs/alt-wpa_supplicant/% > $TMPDIR/lcov-hostapd.info-$POSTFIX &

cd $TMPDIR/alt-hostapd-as/hostapd
lcov -c -d .. 2> lcov.log | sed s%SF:/tmp/logs/alt-[^/]*/%SF:/tmp/logs/alt-wpa_supplicant/% > $TMPDIR/lcov-hostapd-as.info-$POSTFIX &

cd $TMPDIR/alt-hlr_auc_gw/hostapd
lcov -c -d .. 2> lcov.log | sed s%SF:/tmp/logs/alt-[^/]*/%SF:/tmp/logs/alt-wpa_supplicant/% > $TMPDIR/lcov-hlr_auc_gw.info-$POSTFIX &
wait

cd $DIR
if [ "$RESTORE" == "restore" ]; then
    mv $TMPDIR/alt-* $LOGDIR
else
    rm -r $TMPDIR/alt-wpa_supplicant
    rm -r $TMPDIR/alt-hostapd
    rm -r $TMPDIR/alt-hostapd-as
    rm -r $TMPDIR/alt-hlr_auc_gw
fi
