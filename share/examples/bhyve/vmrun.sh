#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
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
#

LOADER=/usr/sbin/bhyveload
BHYVECTL=/usr/sbin/bhyvectl
FBSDRUN=/usr/sbin/bhyve

DEFAULT_MEMSIZE=512M
DEFAULT_CPUS=2
DEFAULT_TAPDEV=tap0
DEFAULT_CONSOLE=stdio

DEFAULT_NIC=virtio-net
DEFAULT_DISK=virtio-blk
DEFAULT_VIRTIO_DISK="./diskdev"
DEFAULT_ISOFILE="./release.iso"

DEFAULT_VNCHOST="127.0.0.1"
DEFAULT_VNCPORT=5900
DEFAULT_VNCSIZE="w=1024,h=768"

errmsg() {
	echo "*** $1"
}

usage() {
	local msg=$1

	echo "Usage: vmrun.sh [-aAEhiTuvw] [-c <CPUs>] [-C <console>]" \
	    "[-d <disk file>]"
	echo "                [-e <name=value>] [-f <path of firmware>]" \
	    "[-F <size>]"
	echo "                [-G [w][address:]port] [-H <directory>]"
	echo "                [-I <location of installation iso>] [-l <loader>]"
	echo "                [-L <VNC IP for UEFI framebuffer>]"
	echo "                [-m <memsize>]" \
	    "[-n <network adapter emulation type>]"
	echo "                [-p <pcidev|bus/slot/func>]"
	echo "                [-P <port>] [-t <tapdev>] <vmname>"
	echo ""
	echo "       -h: display this help message"
	echo "       -a: force memory mapped local APIC access"
	echo "       -A: use AHCI disk emulation instead of ${DEFAULT_DISK}"
	echo "       -c: number of virtual cpus (default: ${DEFAULT_CPUS})"
	echo "       -C: console device (default: ${DEFAULT_CONSOLE})"
	echo "       -d: virtio diskdev file (default: ${DEFAULT_VIRTIO_DISK})"
	echo "       -e: set FreeBSD loader environment variable"
	echo "       -E: Use UEFI mode (amd64 only)"
	echo "       -f: Use a specific boot firmware (e.g., EDK2, U-Boot)"
	echo "       -F: Use a custom UEFI GOP framebuffer size" \
	    "(default: ${DEFAULT_VNCSIZE}) (amd64 only)"
	echo "       -G: bind the GDB stub to the specified address"
	echo "       -H: host filesystem to export to the loader"
	echo "       -i: force boot of the Installation CDROM image"
	echo "       -I: Installation CDROM image location" \
	    "(default: ${DEFAULT_ISOFILE})"
	echo "       -l: the OS loader to use (default: /boot/userboot.so) (amd64 only)"
	echo "       -L: IP address for UEFI GOP VNC server" \
	    "(default: ${DEFAULT_VNCHOST})"
	echo "       -m: memory size (default: ${DEFAULT_MEMSIZE})"
	echo "       -n: network adapter emulation type" \
	    "(default: ${DEFAULT_NIC})"
	echo "       -p: pass-through a host PCI device (e.g ppt0 or" \
	    "bus/slot/func) (amd64 only)"
	echo "       -P: UEFI GOP VNC port (default: ${DEFAULT_VNCPORT})"
	echo "       -t: tap device for virtio-net (default: $DEFAULT_TAPDEV)"
	echo "       -T: Enable tablet device (for UEFI GOP) (amd64 only)"
	echo "       -u: RTC keeps UTC time"
	echo "       -v: Wait for VNC client connection before booting VM"
	echo "       -w: ignore unimplemented MSRs (amd64 only)"
	echo ""
	[ -n "$msg" ] && errmsg "$msg"
	exit 1
}

if [ `id -u` -ne 0 ]; then
	errmsg "This script must be executed with superuser privileges"
	exit 1
fi

kldstat -n vmm > /dev/null 2>&1 
if [ $? -ne 0 ]; then
	errmsg "vmm.ko is not loaded"
	exit 1
fi

platform=$(uname -m)
if [ "${platform}" != amd64 -a "${platform}" != arm64 ]; then
	errmsg "This script is only supported on amd64 and arm64 platforms"
	exit 1
fi

force_install=0
isofile=${DEFAULT_ISOFILE}
memsize=${DEFAULT_MEMSIZE}
console=${DEFAULT_CONSOLE}
cpus=${DEFAULT_CPUS}
nic=${DEFAULT_NIC}
tap_total=0
disk_total=0
disk_emulation=${DEFAULT_DISK}
loader_opt=""
pass_total=0

# EFI-specific options
efi_mode=0
efi_firmware="/usr/local/share/uefi-firmware/BHYVE_UEFI.fd"
vncwait=""
vnchost=${DEFAULT_VNCHOST}
vncport=${DEFAULT_VNCPORT}
vncsize=${DEFAULT_VNCSIZE}
tablet=""

# arm64 only
uboot_firmware="/usr/local/share/u-boot/u-boot-bhyve-arm64/u-boot.bin"

case ${platform} in
amd64)
	bhyverun_opt="-H -P"
	opts="aAc:C:d:e:Ef:F:G:hH:iI:l:L:m:n:p:P:t:Tuvw"
	;;
arm64)
	bhyverun_opt=""
	opts="aAc:C:d:e:f:F:G:hH:iI:L:m:n:P:t:uv"
	;;
esac

while getopts $opts c ; do
	case $c in
	a)
		bhyverun_opt="${bhyverun_opt} -a"
		;;
	A)
		disk_emulation="ahci-hd"
		;;
	c)
		cpus=${OPTARG}
		;;
	C)
		console=${OPTARG}
		;;
	d)
		disk_dev=${OPTARG%%,*}
		disk_opts=${OPTARG#${disk_dev}}
		eval "disk_dev${disk_total}=\"${disk_dev}\""
		eval "disk_opts${disk_total}=\"${disk_opts}\""
		disk_total=$(($disk_total + 1))
		;;
	e)
		loader_opt="${loader_opt} -e ${OPTARG}"
		;;
	E)
		efi_mode=1
		;;
	f)
		firmware="${OPTARG}"
		;;
	F)
		vncsize="${OPTARG}"
		;;
	G)
		bhyverun_opt="${bhyverun_opt} -G ${OPTARG}"
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
	l)
		loader_opt="${loader_opt} -l ${OPTARG}"
		;;
	L)
		vnchost="${OPTARG}"
		;;
	m)
		memsize=${OPTARG}
		;;
	n)
		nic=${OPTARG}
		;;
	p)
		eval "pass_dev${pass_total}=\"${OPTARG}\""
		pass_total=$(($pass_total + 1))
		;;
	P)
		vncport="${OPTARG}"
		;;
	t)
		eval "tap_dev${tap_total}=\"${OPTARG}\""
		tap_total=$(($tap_total + 1))
		;;
	T)
		tablet="-s 30,xhci,tablet"
		;;
	u)	
		bhyverun_opt="${bhyverun_opt} -u"
		;;
	v)
		vncwait=",wait"
		;;
	w)
		bhyverun_opt="${bhyverun_opt} -w"
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
	usage "virtual machine name not specified"
fi

vmname="$1"
if [ -n "${host_base}" ]; then
	loader_opt="${loader_opt} -h ${host_base}"
fi

# If PCI passthru devices are configured then guest memory must be wired
if [ ${pass_total} -gt 0 ]; then
	loader_opt="${loader_opt} -S"
	bhyverun_opt="${bhyverun_opt} -S"
fi

if [ -z "$firmware" ]; then
	case ${platform} in
	amd64)
		if [ ${efi_mode} -ne 0 ]; then
			firmware="${efi_firmware}"
			firmware_pkg="edk2-bhyve"
		fi
		;;
	arm64)
		firmware="${uboot_firmware}"
		firmware_pkg="u-boot-bhyve-arm64"
		;;
	esac
fi

if [ -n "${firmware}" -a ! -f "${firmware}" ]; then
	echo "Error: Firmware file ${firmware} doesn't exist."
	if [ -n "${firmware_pkg}" ]; then
		echo "       Try: pkg install ${firmware_pkg}"
	fi
	exit 1
fi

make_and_check_diskdev()
{
    local virtio_diskdev="$1"
    # Create the virtio diskdev file if needed
    if [ ! -e ${virtio_diskdev} ]; then
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

first_diskdev="$disk_dev0"

${BHYVECTL} --vm=${vmname} --destroy > /dev/null 2>&1

while [ 1 ]; do

	file -s ${first_diskdev} | grep "boot sector" > /dev/null
	rc=$?
	if [ $rc -ne 0 ]; then
		file -s ${first_diskdev} | \
		    grep ": Unix Fast File sys" > /dev/null
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
		BOOTDISKS="-d ${isofile}"
		installer_opt="-s 31:0,ahci-cd,${isofile}"
	else
		BOOTDISKS=""
		i=0
		while [ $i -lt $disk_total ] ; do
			eval "disk=\$disk_dev${i}"
			if [ -r ${disk} ] ; then
				BOOTDISKS="$BOOTDISKS -d ${disk} "
			fi
			i=$(($i + 1))
		done
		installer_opt=""
	fi

	if [ ${platform} = amd64 -a ${efi_mode} -eq 0 ]; then
		${LOADER} -c ${console} -m ${memsize} ${BOOTDISKS} \
		    ${loader_opt} ${vmname}
		bhyve_exit=$?
		if [ $bhyve_exit -ne 0 ]; then
			break
		fi
	fi

	#
	# Build up args for additional tap and disk devices now.
	#
	devargs="-s 0:0,hostbridge"  # accumulate disk/tap args here
	case ${platform} in
	amd64)
		console_opt="-l com1,${console}"
		devargs="$devargs -s 1:0,lpc "
		nextslot=2  # slot 0 is hostbridge, slot 1 is lpc
		;;
	arm64)
		console_opt="-o console=${console}"
		devargs="$devargs -o bootrom=${firmware} "
		nextslot=1  # slot 0 is hostbridge
		;;
	esac

	i=0
	while [ $i -lt $disk_total ] ; do
	    eval "disk=\$disk_dev${i}"
	    eval "opts=\$disk_opts${i}"
	    make_and_check_diskdev "${disk}"
	    devargs="$devargs -s $nextslot:0,$disk_emulation,${disk}${opts} "
	    nextslot=$(($nextslot + 1))
	    i=$(($i + 1))
	done

	i=0
	while [ $i -lt $tap_total ] ; do
	    eval "tapname=\$tap_dev${i}"
	    devargs="$devargs -s $nextslot:0,${nic},${tapname} "
	    nextslot=$(($nextslot + 1))
	    i=$(($i + 1))
	done

	i=0
	while [ $i -lt $pass_total ] ; do
		eval "pass=\$pass_dev${i}"
		bsfform="$(echo "${pass}" | grep "^[0-9]\+/[0-9]\+/[0-9]\+$")"
		if [ -z "${bsfform}" ]; then
			bsf="$(pciconf -l "${pass}" 2>/dev/null)"
			if [ $? -ne 0 ]; then
				errmsg "${pass} is not a host PCI device"
				exit 1
			fi
			bsf="$(echo "${bsf}" | awk -F: '{print $2"/"$3"/"$4}')"
		else
			bsf="${pass}"
		fi
		devargs="$devargs -s $nextslot:0,passthru,${bsf} "
		nextslot=$(($nextslot + 1))
		i=$(($i + 1))
        done

	efiargs=""
	if [ ${efi_mode} -gt 0 ]; then
		efiargs="-s 29,fbuf,tcp=${vnchost}:${vncport},"
		efiargs="${efiargs}${vncsize}${vncwait}"
		efiargs="${efiargs} -l bootrom,${firmware}"
		efiargs="${efiargs} ${tablet}"
	fi

	${FBSDRUN} -c ${cpus} -m ${memsize} ${bhyverun_opt}		\
		${efiargs}						\
		${devargs}						\
		${console_opt}						\
		${installer_opt}					\
		${vmname}

	bhyve_exit=$?
	# bhyve returns the following status codes:
	#  0 - VM has been reset
	#  1 - VM has been powered off
	#  2 - VM has been halted
	#  3 - VM generated a triple fault
	#  all other non-zero status codes are errors
	#
	if [ $bhyve_exit -ne 0 ]; then
		break
	fi
done


case $bhyve_exit in
	0|1|2)
		# Cleanup /dev/vmm entry when bhyve did not exit
		# due to an error.
		${BHYVECTL} --vm=${vmname} --destroy > /dev/null 2>&1
		;;
esac

exit $bhyve_exit
