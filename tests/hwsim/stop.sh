#!/bin/sh

if pidof wpa_supplicant hostapd valgrind.bin hlr_auc_gw > /dev/null; then
    RUNNING=yes
else
    RUNNING=no
fi

sudo killall -q hostapd
sudo killall -q wpa_supplicant
for i in `pidof valgrind.bin`; do
    if ps $i | grep -q -E "wpa_supplicant|hostapd"; then
	sudo kill $i
    fi
done
sudo killall -q wlantest
if grep -q hwsim0 /proc/net/dev; then
    sudo ifconfig hwsim0 down
fi

sudo killall -q hlr_auc_gw

if [ "$RUNNING" = "yes" ]; then
    # give some time for hostapd and wpa_supplicant to complete deinit
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
	if ! pidof wpa_supplicant hostapd valgrind.bin hlr_auc_gw > /dev/null; then
	    break
	fi
	if [ $i -gt 10 ]; then
	    echo "Waiting for processes to exit (1)"
	    sleep 1
	else
	    sleep 0.06
	fi
    done
fi

if pidof wpa_supplicant hostapd hlr_auc_gw > /dev/null; then
    echo "wpa_supplicant/hostapd/hlr_auc_gw did not exit - try to force them to die"
    sudo killall -9 -q hostapd
    sudo killall -9 -q wpa_supplicant
    sudo killall -9 -q hlr_auc_gw
    for i in `seq 1 5`; do
	if pidof wpa_supplicant hostapd hlr_auc_gw > /dev/null; then
	    echo "Waiting for processes to exit (2)"
	    sleep 1
	else
	    break
	fi
    done
fi

for i in `pidof valgrind.bin`; do
    if ps $i | grep -q -E "wpa_supplicant|hostapd"; then
	echo "wpa_supplicant/hostapd(valgrind) did not exit - try to force it to die"
	sudo kill -9 $i
    fi
done

count=0
for i in /tmp/wpas-wlan0 /tmp/wpas-wlan1 /tmp/wpas-wlan2 /tmp/wpas-wlan5 /var/run/hostapd-global /tmp/hlr_auc_gw.sock /tmp/wpa_ctrl_* /tmp/eap_sim_db_*; do
    count=$(($count + 1))
    if [ $count -lt 7 -a -e $i ]; then
	echo "Waiting for ctrl_iface $i to disappear"
	sleep 1
    fi
    if [ -e $i ]; then
	echo "Control interface file $i exists - remove it"
	sudo rm $i
    fi
done

if grep -q mac80211_hwsim /proc/modules 2>/dev/null ; then
    sudo rmmod mac80211_hwsim
    sudo rmmod mac80211
    sudo rmmod cfg80211
    # wait at the end to avoid issues starting something new immediately after
    # this script returns
    sleep 1
fi
