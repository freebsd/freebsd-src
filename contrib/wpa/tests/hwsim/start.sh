#!/bin/sh

DIR="$( cd "$( dirname "$0" )" && pwd )"
WPAS=$DIR/../../wpa_supplicant/wpa_supplicant
WPACLI=$DIR/../../wpa_supplicant/wpa_cli
HAPD=$DIR/../../hostapd/hostapd
HAPD_AS=$DIR/../../hostapd/hostapd
HAPDCLI=$DIR/../../hostapd/hostapd_cli
WLANTEST=$DIR/../../wlantest/wlantest
HLR_AUC_GW=$DIR/../../hostapd/hlr_auc_gw

if [ -z "$LOGDIR" ] ; then
    DATE="$(date +%s)"
    LOGDIR="$DIR/logs/$DATE"
    mkdir -p $LOGDIR
else
    if [ -e $LOGDIR/alt-wpa_supplicant/wpa_supplicant/wpa_supplicant ]; then
	WPAS=$LOGDIR/alt-wpa_supplicant/wpa_supplicant/wpa_supplicant
	WPACLI=$LOGDIR/alt-wpa_supplicant/wpa_supplicant/wpa_cli
	# extra code coverage
	$WPAS > /dev/null 2>&1
	$WPAS -efoo -Ifoo -mfoo -ofoo -Ofoo -pfoo -Pfoo -h > /dev/null 2>&1
	$WPAS -bfoo -B -Cfoo -q -W -N -L > /dev/null 2>&1
	$WPAS -T -v > /dev/null 2>&1
	$WPAS -u -z > /dev/null 2>&1
    fi
    if [ -e $LOGDIR/alt-hostapd/hostapd/hostapd ]; then
	HAPD=$LOGDIR/alt-hostapd/hostapd/hostapd
	HAPDCLI=$LOGDIR/alt-hostapd/hostapd/hostapd_cli
	# extra code coverage
	$HAPD > /dev/null 2>&1
	$HAPD -v > /dev/null 2>&1
	$HAPD -B -efoo -Pfoo -T -bfoo -h > /dev/null 2>&1
	$HAPD -ufoo > /dev/null 2>&1
	$HAPD -u00:11:22:33:44:55 > /dev/null 2>&1
	$HAPD -gfoo > /dev/null 2>&1
	$HAPD -Gfoo-not-exists > /dev/null 2>&1
	$HAPD -z > /dev/null 2>&1
	$HAPD -i foo1,foo2,foo3 > /dev/null 2>&1
    fi
    if [ -e $LOGDIR/alt-hostapd-as/hostapd/hostapd ]; then
	HAPD_AS=$LOGDIR/alt-hostapd-as/hostapd/hostapd
    fi
    if [ -e $LOGDIR/alt-hlr_auc_gw/hostapd/hlr_auc_gw ]; then
	HLR_AUC_GW=$LOGDIR/alt-hlr_auc_gw/hostapd/hlr_auc_gw
	# extra code coverage
	$HLR_AUC_GW > /dev/null 2>&1
	$HLR_AUC_GW -Dfoo -i7 -sfoo -h > /dev/null 2>&1
	$HLR_AUC_GW -i100 > /dev/null 2>&1
	$HLR_AUC_GW -z > /dev/null 2>&1
    fi
fi

LOGBASEDIR="$( cd "$(dirname "$LOGDIR")" && pwd )"
if test "$LOGBASEDIR" = "$DIR/logs" -a -w "$LOGBASEDIR" ; then
    rm -rf "$LOGBASEDIR/current"
    ln -sf "$(basename "$LOGDIR")" "$LOGBASEDIR/current"
fi

if groups | tr ' ' "\n" | grep -q ^admin$; then
    GROUP=admin
elif groups | tr ' ' "\n" | grep -q ^wheel$; then
    GROUP=wheel
else
    GROUP=adm
fi

for i in 0 1 2; do
    sed "s/ GROUP=.*$/ GROUP=$GROUP/" "$DIR/p2p$i.conf" > "$LOGDIR/p2p$i.conf"
done

sed "s/group=admin/group=$GROUP/;s%LOGDIR%$LOGDIR%g" "$DIR/auth_serv/as.conf" > "$LOGDIR/as.conf"
sed "s/group=admin/group=$GROUP/;s%LOGDIR%$LOGDIR%g" "$DIR/auth_serv/as2.conf" > "$LOGDIR/as2.conf"

unset VM
if [ "$1" = "VM" ]; then
    VM="y"
    shift
fi

if [ "$1" = "valgrind" ]; then
    VALGRIND=y
    VALGRIND_WPAS="valgrind --log-file=$LOGDIR/valgrind-wlan%d --leak-check=full"
    VALGRIND_HAPD="valgrind --log-file=$LOGDIR/valgrind-hostapd --leak-check=full"
    chmod -f a+rx $WPAS
    chmod -f a+rx $HAPD
    chmod -f a+rx $HAPD_AS
    HAPD_AS="valgrind --log-file=$LOGDIR/valgrind-auth-serv --leak-check=full $HAPD_AS"
    shift
else
    unset VALGRIND
    VALGRIND_WPAS=
    VALGRIND_HAPD=
fi

if [ "$1" = "trace" ]; then
    TRACE="T"
    shift
else
    TRACE=""
fi

$DIR/stop.sh

TMP=$1
if [ x${TMP%=[0-9]*} = "xchannels" ]; then
	NUM_CH=${TMP#channels=}
	shift
else
	NUM_CH=1
fi

test -d /sys/module/mac80211_hwsim || sudo modprobe mac80211_hwsim radios=7 channels=$NUM_CH support_p2p_device=0 dyndbg=+p

sudo ifconfig hwsim0 up
sudo $WLANTEST -i hwsim0 -n $LOGDIR/hwsim0.pcapng -c -dtN -L $LOGDIR/hwsim0 &
for i in 0 1 2; do
    DBUSARG=""
    if [ $i = "0" ] && ([ -r /var/run/dbus/pid ] || [ -r /var/run/dbus/system_bus_socket ]); then
	if $WPAS | grep -q -- -u; then
	    DBUSARG="-u"
	fi
    fi
    sudo $(printf -- "$VALGRIND_WPAS" $i) $WPAS -g /tmp/wpas-wlan$i -G$GROUP -Dnl80211 -iwlan$i -c $LOGDIR/p2p$i.conf \
         -ddKt$TRACE -f $LOGDIR/log$i $DBUSARG &
done
sudo $(printf -- "$VALGRIND_WPAS" 5) $WPAS -g /tmp/wpas-wlan5 -G$GROUP \
    -ddKt$TRACE -f $LOGDIR/log5 &
sudo $VALGRIND_HAPD $HAPD -ddKt$TRACE -g /var/run/hostapd-global -G $GROUP -f $LOGDIR/hostapd &
HPID=$!

if [ -z "$VM" ]; then
    # Sleep a bit, otherwise pgrep may run before the child is forked
    sleep 0.1
    pgrep -P $HPID > $LOGDIR/hostapd-test.pid
else
    echo $HPID > $LOGDIR/hostapd-test.pid
fi

if [ -x $HLR_AUC_GW ]; then
    cp $DIR/auth_serv/hlr_auc_gw.milenage_db $LOGDIR/hlr_auc_gw.milenage_db
    sudo $HLR_AUC_GW -u -m $LOGDIR/hlr_auc_gw.milenage_db -g $DIR/auth_serv/hlr_auc_gw.gsm > $LOGDIR/hlr_auc_gw &
fi

openssl ocsp -index $DIR/auth_serv/index.txt \
    -rsigner $DIR/auth_serv/ocsp-responder.pem \
    -rkey $DIR/auth_serv/ocsp-responder.key \
    -CA $DIR/auth_serv/ca.pem \
    -issuer $DIR/auth_serv/ca.pem \
    -verify_other $DIR/auth_serv/ca.pem -trust_other \
    -ndays 7 \
    -reqin $DIR/auth_serv/ocsp-req.der \
    -respout $LOGDIR/ocsp-server-cache.der > $LOGDIR/ocsp.log 2>&1
if [ ! -r $LOGDIR/ocsp-server-cache.der ]; then
    cp $DIR/auth_serv/ocsp-server-cache.der $LOGDIR/ocsp-server-cache.der
fi

touch $LOGDIR/hostapd.db
sudo $HAPD_AS -ddKt $LOGDIR/as.conf $LOGDIR/as2.conf > $LOGDIR/auth_serv &

# wait for programs to be fully initialized
for i in 0 1 2 3 4 5 6 7 8 9; do
    if [ -e /tmp/wpas-wlan0 ]; then
	break
    fi
    sleep 0.05
done
for i in 0 1 2; do
    for j in `seq 1 10`; do
	if $WPACLI -g /tmp/wpas-wlan$i ping | grep -q PONG; then
	    break
	fi
	if [ $j = "10" ]; then
	    echo "Could not connect to /tmp/wpas-wlan$i"
	    exit 1
	fi
	sleep 1
    done
done

for j in `seq 1 10`; do
    if $WPACLI -g /var/run/hostapd-global ping | grep -q PONG; then
	break
    fi
    if [ $j = "10" ]; then
	echo "Could not connect to /var/run/hostapd-global"
	exit 1
    fi
    sleep 1
done

for j in `seq 1 10`; do
    if $HAPDCLI -i as ping | grep -q PONG; then
	break
    fi
    if [ $j = "10" ]; then
	echo "Could not connect to hostapd-as-RADIUS-server"
	exit 1
    fi
    sleep 1
done

if [ $USER = "0" -o $USER = "root" ]; then
    exit 0
fi

sleep 0.75
sudo chown -f $USER $LOGDIR/hwsim0.pcapng $LOGDIR/hwsim0 $LOGDIR/log* $LOGDIR/hostapd
if [ "x$VALGRIND" = "xy" ]; then
    sudo chown -f $USER $LOGDIR/*valgrind*
fi

exit 0
