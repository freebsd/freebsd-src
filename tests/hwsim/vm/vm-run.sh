#!/bin/bash

cd "$(dirname $0)"

if [ -z "$TESTDIR" ] ; then
	TESTDIR=$(pwd)/../
fi

if [ -n "$HWSIM_TEST_LOG_DIR" ] ; then
	LOGS="$HWSIM_TEST_LOG_DIR"
else
	LOGS=/tmp/hwsim-test-logs
fi

# increase the memory size if you want to run with valgrind, 512 MB works
MEMORY=256

# Some ubuntu systems (notably 12.04) have issues with this - since the guest
# mounts as read-only it should be safe to not specify ,readonly. Override in
# vm-config if needed (see below)
ROTAG=,readonly

# set this to ttyS0 to see kvm messages (if something doesn't work)
KVMOUT=ttyS1

# you can set EPATH if you need anything extra in $PATH inside the VM
#EPATH=/some/dir

# extra KVM arguments, e.g., -s for gdbserver
#KVMARGS=-s

# number of channels each hwsim device supports
CHANNELS=1

test -f vm-config && . vm-config
test -f ~/.wpas-vm-config && . ~/.wpas-vm-config

if [ -z "$KERNEL" ] && [ -z "$KERNELDIR" ] ; then
	echo "You need to set a KERNEL or KERNELDIR (in the environment or vm-config)"
	exit 2
fi
if [ -z "$KERNEL" ] ; then
	if [ -e $KERNELDIR/arch/x86_64/boot/bzImage ]; then
		KERNEL=$KERNELDIR/arch/x86_64/boot/bzImage
	elif [ -e $KERNELDIR/linux ]; then
		KERNEL=$KERNELDIR/linux
	else
		echo "No suitable kernel image found from KERNELDIR"
		exit 2
	fi
fi
if [ ! -e $KERNEL ]; then
	echo "Kernel image not found: $KERNEL"
	exit 2
fi


CMD=$TESTDIR/vm/inside.sh

unset RUN_TEST_ARGS
TIMESTAMP=$(date +%s)
DATE=$TIMESTAMP
CODECOV=no
TIMEWARP=0
TELNET_QEMU=
TELNET_ARG=0
CODECOV_DIR=
while [ "$1" != "" ]; do
	case $1 in
		--timestamp ) shift
			TIMESTAMP=$1
			shift
			;;
		--ext ) shift
			DATE=$TIMESTAMP.$1
			shift
			;;
		--codecov ) shift
			CODECOV=yes
			;;
		--codecov_dir ) shift
			CODECOV_DIR=$1
			shift
			;;
		--timewrap ) shift
			TIMEWARP=1
			;;
		--telnet ) shift
			TELNET_ARG=1
			TELNET_QEMU="-net nic,model=virtio -net user,id=telnet,restrict=on,net=172.16.0.0/24,hostfwd=tcp:127.0.0.1:$1-:23"
			shift
			;;
		* )
			RUN_TEST_ARGS="$RUN_TEST_ARGS$1 "
			shift
			;;
	esac
done

LOGDIR=$LOGS/$DATE
mkdir -p $LOGDIR
rm -f $LOGS/latest
ln -s $LOGDIR $LOGS/latest

if [ -n "$CODECOV_DIR" ]; then
    cp -a $CODECOV_DIR/alt-wpa_supplicant $LOGDIR
    cp -a $CODECOV_DIR/alt-hostapd $LOGDIR
    cp -a $CODECOV_DIR/alt-hostapd-as $LOGDIR
    cp -a $CODECOV_DIR/alt-hlr_auc_gw $LOGDIR
elif [ $CODECOV = "yes" ]; then
    ./build-codecov.sh $LOGDIR || exit 1
else
    CODECOV=no
fi

echo "Starting test run in a virtual machine"

if [ -x $KERNEL ]; then
	unset KVM
else
	KVM=kvm
	for kvmprog in kvm qemu-kvm; do
		if $kvmprog --version &> /dev/null; then
			KVM=$kvmprog
			break
		fi
	done
fi

argsfile=$(mktemp)
if [ $? -ne 0 ] ; then
	exit 2
fi
function finish {
	rm -f $argsfile
}
trap finish EXIT

if [ -z $KVM ]; then
	RUN_TEST_ARGS="--long $RUN_TEST_ARGS"
fi
echo "$RUN_TEST_ARGS" > $argsfile

A="mac80211_hwsim.support_p2p_device=0 "
A+="mac80211_hwsim.channels=$CHANNELS "
A+="mac80211_hwsim.radios=7 "
A+="cfg80211.dyndbg=+p "
A+="mac80211.dyndbg=+p "
A+="mac80211_hwsim.dyndbg=+p "
A+="init=$CMD "
A+="testdir=$TESTDIR "
A+="timewarp=$TIMEWARP "
A+="TELNET=$TELNET_ARG "
A+="EPATH=$EPATH "
A+="ARGS=$argsfile "
A+="console=$KVMOUT "
A+="ro"

if [ -z $KVM ]; then
	$KERNEL \
	     mem=${MEMORY}M \
	     LOGDIR=$LOGDIR \
	     time-travel=inf-cpu \
	     $A \
	     root=none hostfs=/ rootfstype=hostfs rootflags=/ \
	     ssl0=fd:0,fd:1 \
	     ssl1=fd:100 \
	     ssl-non-raw \
	     100<>$LOGDIR/console 2>&1 | \
	    sed -u '0,/VM has started up/d'
else
	$KVM \
	    -kernel $KERNEL \
	    -smp 4 \
	    $KVMARGS \
	    -m $MEMORY \
	    -nographic \
	    -fsdev local,security_model=none,id=fsdev-root,path=/$ROTAG \
	    -device virtio-9p-pci,id=fs-root,fsdev=fsdev-root,mount_tag=/dev/root \
	    -fsdev local,security_model=none,id=fsdev-logs,path="$LOGDIR",writeout=immediate \
	    -device virtio-9p-pci,id=fs-logs,fsdev=fsdev-logs,mount_tag=logshare \
	    -monitor null \
	    -serial stdio \
	    -serial file:$LOGDIR/console \
	    $TELNET_QEMU \
	    -append "$A root=/dev/root rootflags=trans=virtio,version=9p2000.u rootfstype=9p" | \
	    sed -u '0,/VM has started up/d'
fi

if [ $CODECOV = "yes" ]; then
    echo "Preparing code coverage reports"
    ./process-codecov.sh $LOGDIR "" restore
    ./combine-codecov.sh $LOGDIR lcov
fi

echo
echo "Test run completed"
echo "Logfiles are at $LOGDIR ($LOGS/latest)"
if [ $CODECOV = "yes" ]; then
    echo "Code coverage report:"
    echo "file://$LOGDIR/lcov/index.html"
fi
