#!/bin/bash

LOGDIR=$1
DIR=$PWD
TMPDIR=/tmp/logs

if [ -e $TMPDIR ]; then
	echo "$TMPDIR exists - cannot prepare build trees"
	exit 1
fi
mkdir $TMPDIR
echo "Preparing separate build trees for hostapd/wpa_supplicant"
cd ../../..
git archive --format=tar --prefix=hostap/ HEAD > $TMPDIR/hostap.tar
cd $DIR
cat ../../../wpa_supplicant/.config > $TMPDIR/wpa_supplicant.config
echo "CONFIG_CODE_COVERAGE=y" >> $TMPDIR/wpa_supplicant.config
cat ../../../hostapd/.config > $TMPDIR/hostapd.config
echo "CONFIG_CODE_COVERAGE=y" >> $TMPDIR/hostapd.config

cd $TMPDIR
tar xf hostap.tar
mv hostap alt-wpa_supplicant
mv wpa_supplicant.config alt-wpa_supplicant/wpa_supplicant/.config
tar xf hostap.tar
mv hostap alt-hostapd
cp hostapd.config alt-hostapd/hostapd/.config
tar xf hostap.tar
mv hostap alt-hostapd-as
cp hostapd.config alt-hostapd-as/hostapd/.config
tar xf hostap.tar
mv hostap alt-hlr_auc_gw
mv hostapd.config alt-hlr_auc_gw/hostapd/.config
rm hostap.tar

cd $TMPDIR/alt-wpa_supplicant/wpa_supplicant
echo "Building wpa_supplicant"
make -j8 > /dev/null

cd $TMPDIR/alt-hostapd/hostapd
echo "Building hostapd"
make -j8 hostapd hostapd_cli > /dev/null

cd $TMPDIR/alt-hostapd-as/hostapd
echo "Building hostapd (AS)"
make -j8 hostapd hostapd_cli > /dev/null

cd $TMPDIR/alt-hlr_auc_gw/hostapd
echo "Building hlr_auc_gw"
make -j8 hlr_auc_gw > /dev/null

cd $DIR

mv $TMPDIR/alt-wpa_supplicant $LOGDIR
mv $TMPDIR/alt-hostapd $LOGDIR
mv $TMPDIR/alt-hostapd-as $LOGDIR
mv $TMPDIR/alt-hlr_auc_gw $LOGDIR
