#!/bin/sh
#
# Copyright (c) 2013 NetApp, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

LOADER=/usr/sbin/bhyveload
BHYVECTL=/usr/sbin/bhyvectl
FBSDRUN=/usr/sbin/bhyve

DEFAULT_MEMSIZE=512M
DEFAULT_CPUS=2
DEFAULT_TAPDEV=tap0
DEFAULT_CONSOLE=stdio

DEFAULT_VIRTIO_DISK="./diskdev"
DEFAULT_ISOFILE="./release.iso"

usage() {
	echo "Usage: vmrun.sh [-ahi] [-c <CPUs>] [-C <console>] [-d <disk file>]"
	echo "                [-e <name=value>] [-g <gdbport> ] [-H <directory>]"
	echo "                [-I <location of installation iso>] [-m <memsize>]"
	echo "                [-t <tapdev>] <vmname>"
	echo ""
	echo "       -h: display this help message"
	echo "       -a: force memory mapped local APIC access"
	echo "       -c: number of virtual cpus (default is ${DEFAULT_CPUS})"
	echo "       -C: console device (default is ${DEFAULT_CONSOLE})"
	echo "       -d: virtio diskdev file (default is ${DEFAULT_VIRTIO_DISK})"
	echo "       -e: set FreeBSD loader environment variable"
	echo "       -g: listen for connection from kgdb at <gdbport>"
	echo "       -H: host filesystem to export to the loader"
	echo "       -i: force boot of the Installation CDROM image"
	echo "       -I: Installation CDROM image location (default is ${DEFAULT_ISOFILE})"
	echo "       -m: memory size (default is ${DEFAULT_MEMSIZE})"
	echo "       -t: tap device for virtio-net (default is $DEFAULT_TAPDEV)"
	echo ""
	echo "       This script needs to be executed with superuser privileges"
	echo ""
	exit 1
}

if [ `id -u` -ne 0 ]; then
	usage
fi

kldstat -n vmm > /dev/null 2>&1 
if [ $? -ne 0 ]; then
	echo "vmm.ko is not loaded!"
	exit 1
fi

force_install=0
isofile=${DEFAULT_ISOFILE}
memsize=${DEFAULT_MEMSIZE}
console=${DEFAULT_CONSOLE}
cpus=${DEFAULT_CPUS}
tap_total=0
disk_total=0
apic_opt=""
gdbport=0
loader_opt=""

while getopts ac:C:d:e:g:hH:iI:m:t: c ; do
	case $c in
	a)
		apic_opt="-a"
		;;
	c)
		cpus=${OPTARG}
		;;
	C)
		console=${OPTARG}
		;;
	d)
		eval "disk_dev${disk_total}=\"${OPTARG}\""
		disk_total=$(($disk_total + 1))
		;;
	e)
		loader_opt="${loader_opt} -e ${OPTARG}"
		;;
	g)	
		gdbport=${OPTARG}
		;;
	H)
		host_base=`realpath ${OPTARG}`
		;;
	i)
		force_install=1
		;;
	I)
		isofile=${OPTARG}
		;;
	m)
		memsize=${OPTARG}
		;;
	t)
		eval "tap_dev${tap_total}=\"${OPTARG}\""
		tap_total=$(($tap_total + 1))
		;;
	*)
		usage
		;;
	esac
done

if [ $tap_total -eq 0 ] ; then
    tap_total=1
    tap_dev0="${DEFAULT_TAPDEV}"
fi
if [ $disk_total -eq 0 ] ; then
    disk_total=1
    disk_dev0="${DEFAULT_VIRTIO_DISK}"

fi

shift $((${OPTIND} - 1))

if [ $# -ne 1 ]; then
	usage
fi

vmname="$1"
if [ -n "${host_base}" ]; then
	loader_opt="${loader_opt} -h ${host_base}"
fi

make_and_check_diskdev()
{
    local virtio_diskdev="$1"
    # Create the virtio diskdev file if needed
    if [ ! -f ${virtio_diskdev} ]; then
	    echo "virtio disk device file \"${virtio_diskdev}\" does not exist."
	    echo "Creating it ..."
	    truncate -s 8G ${virtio_diskdev} > /dev/null
    fi

    if [ ! -r ${virtio_diskdev} ]; then
	    echo "virtio disk device file \"${virtio_diskdev}\" is not readable"
	    exit 1
    fi

    if [ ! -w ${virtio_diskdev} ]; then
	    echo "virtio disk device file \"${virtio_diskdev}\" is not writable"
	    exit 1
    fi
}

echo "Launching virtual machine \"$vmname\" ..."

virtio_diskdev="$disk_dev0"

${BHYVECTL} --vm=${vmname} --destroy > /dev/null 2>&1

while [ 1 ]; do

	file -s ${virtio_diskdev} | grep "boot sector" > /dev/null
	rc=$?
	if [ $rc -ne 0 ]; then
		file -s ${virtio_diskdev} | grep ": Unix Fast File sys" > /dev/null
		rc=$?
	fi
	if [ $rc -ne 0 ]; then
		need_install=1
	else
		need_install=0
	fi

	if [ $force_install -eq 1 -o $need_install -eq 1 ]; then
		if [ ! -r ${isofile} ]; then
			echo -n "Installation CDROM image \"${isofile}\" "
			echo    "is not readable"
			exit 1
		fi
		BOOTDISK=${isofile}
		installer_opt="-s 31:0,virtio-blk,${BOOTDISK}"
	else
		BOOTDISK=${virtio_diskdev}
		installer_opt=""
	fi

	${LOADER} -c ${console} -m ${memsize} -d ${BOOTDISK} ${loader_opt} \
		${vmname}
	if [ $? -ne 0 ]; then
		break
	fi

	#
	# Build up args for additional tap and disk devices now.
	#
	nextslot=2  # slot 0 is hostbridge, slot 1 is lpc
	devargs=""  # accumulate disk/tap args here
	i=0
	while [ $i -lt $tap_total ] ; do
	    eval "tapname=\$tap_dev${i}"
	    devargs="$devargs -s $nextslot:0,virtio-net,${tapname} "
	    nextslot=$(($nextslot + 1))
	    i=$(($i + 1))
	done

	i=0
	while [ $i -lt $disk_total ] ; do
	    eval "disk=\$disk_dev${i}"
	    make_and_check_diskdev "${disk}"
	    devargs="$devargs -s $nextslot:0,virtio-blk,${disk} "
	    nextslot=$(($nextslot + 1))
	    i=$(($i + 1))
	done

	${FBSDRUN} -c ${cpus} -m ${memsize} ${apic_opt} -A -H -P	\
		-g ${gdbport}						\
		-s 0:0,hostbridge					\
		-s 1:0,lpc						\
		${devargs}						\
		-l com1,${console}					\
		${installer_opt}					\
		${vmname}

	# bhyve returns the following status codes:
	#  0 - VM has been reset
	#  1 - VM has been powered off
	#  2 - VM has been halted
	#  3 - VM generated a triple fault
	#  all other non-zero status codes are errors
	#
	if [ $? -ne 0 ]; then
		break
	fi
done

exit 99
