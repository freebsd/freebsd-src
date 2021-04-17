#!/bin/sh

set -e

cd $(dirname $0)

usage()
{
	echo "$0 [-c | --codecov] [-f | --force-config]"
	exit 1
}

use_lcov=0
force_config=0
while [ "$1" != "" ]; do
	case $1 in
		-c | --codecov ) shift
			echo "$0: use code coverage specified"
			use_lcov=1
			;;
		-f | --force-config ) shift
			force_config=1
			echo "$0: force copy config specified"
			;;
		* ) usage
	esac
done

JOBS=`nproc`
if [ -z "$ABC" ]; then
    JOBS=8
fi

echo "Building TNC testing tools"
cd tnc
make QUIET=1 -j$JOBS

echo "Building wlantest"
cd ../../../wlantest
make QUIET=1 -j$JOBS > /dev/null

echo "Building hs20-osu-client"
cd ../hs20/client/
make QUIET=1 CONFIG_NO_BROWSER=1

echo "Building hostapd"
cd ../../hostapd
if [ ! -e .config -o $force_config -eq 1 ]; then
    if ! cmp ../tests/hwsim/example-hostapd.config .config >/dev/null 2>&1 ; then
      cp ../tests/hwsim/example-hostapd.config .config
    fi
fi

if [ $use_lcov -eq 1 ]; then
    if ! grep -q CONFIG_CODE_COVERAGE .config; then
	    echo CONFIG_CODE_COVERAGE=y >> .config
    else
	    echo "CONFIG_CODE_COVERAGE already exists in hostapd/.config. Ignore"
    fi
fi

make QUIET=1 -j$JOBS hostapd hostapd_cli hlr_auc_gw

echo "Building wpa_supplicant"
cd ../wpa_supplicant
if [ ! -e .config -o $force_config -eq 1 ]; then
    if ! cmp ../tests/hwsim/example-wpa_supplicant.config .config >/dev/null 2>&1 ; then
      cp ../tests/hwsim/example-wpa_supplicant.config .config
    fi
fi

if [ $use_lcov -eq 1 ]; then
    if ! grep -q CONFIG_CODE_COVERAGE .config; then
	    echo CONFIG_CODE_COVERAGE=y >> .config
    else
	    echo "CONFIG_CODE_COVERAGE already exists in wpa_supplicant/.config. Ignore"
    fi
fi

if [ -z $FIPSLD_CC ]; then
export FIPSLD_CC=gcc
fi
make QUIET=1 -j$JOBS
