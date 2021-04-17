#!/bin/bash

LOGDIR=$1
if [ -n "$2" ]; then
    ODIR=$2
else
    ODIR=.
fi
TMPDIR=/tmp/logs

mv $LOGDIR/alt-* $TMPDIR

cd $TMPDIR
args=""
for i in lcov-*.info-*; do
    args="$args -a $i"
done

lcov $args -o $LOGDIR/combined.info > $LOGDIR/combined-lcov.log 2>&1
cat $LOGDIR/combined.info |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/bits\/byteswap.h$\)/\1/};/^SF:.*\/bits\/byteswap.h$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/openssl\/x509.h$\)/\1/};/^SF:.*\/openssl\/x509.h$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/openssl\/x509v3.h$\)/\1/};/^SF:.*\/openssl\/x509v3.h$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/common\/wpa_ctrl.c$\)/\1/};/^SF:.*\/common\/wpa_ctrl.c$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/common\/cli.c$\)/\1/};/^SF:.*\/common\/cli.c$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/utils\/edit.c$\)/\1/};/^SF:.*\/utils\/edit.c$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*_module_tests.c$\)/\1/};/^SF:.*_module_tests.c$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*\/hostapd\/hostapd_cli.c$\)/\1/};/^SF:.*\/hostapd\/hostapd_cli.c$/,/^end_of_record$/d" |
    sed "/^TN:$/{N;s/TN:\n\(SF:.*wpa_supplicant\/wpa_cli.c$\)/\1/};/^SF:.*wpa_supplicant\/wpa_cli.c$/,/^end_of_record$/d" > $LOGDIR/combined.info.filtered

cd $LOGDIR
genhtml -t "wpa_supplicant/hostapd combined for hwsim test run $(date +%s)" combined.info.filtered --output-directory $ODIR > lcov.log 2>&1

rm -r /tmp/logs/alt-wpa_supplicant
rm -r /tmp/logs/alt-hostapd
rm -r /tmp/logs/alt-hostapd-as
rm -r /tmp/logs/alt-hlr_auc_gw
rm /tmp/logs/lcov-*info-*
rmdir /tmp/logs
