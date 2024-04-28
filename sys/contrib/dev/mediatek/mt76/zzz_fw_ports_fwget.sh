#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 The FreeBSD Foundation
#
# This software was developed by Björn Zeeb
# under sponsorship from the FreeBSD Foundation.
#
# This is neither efficient nor elegant but we need it few times
# a year and it does the job.
#
#
# USAGE: please check out the correct tag/hash for ports in the
# linux-firmware.git repository you point this script to.
#
# Valid flavors: mt7915 mt792x {for the drivers with x=[15]} mt7996
# To add a new one you need to add the mappings in the help functions.
#

set -e

DRIVER=mt76
FWSUBDIR=mediatek
CHECKFILE=mt792x.h

################################################################################
#
# Check pre-reqs
#
if [ $# -ne 1 ]; then
	printf "USAGE: %s /path/to/linux-firmware.git\n" $0 >&2
	exit 1
fi

if [ ! -e ${CHECKFILE} ]; then
	printf "ERROR: run from %s driver directory; no %s.c here\n" ${DRIVER} ${CHECKFILE} >&2
	exit 1
fi

LFWDIR=${1}
if test ! -d ${LFWDIR} -o ! -e ${LFWDIR}/WHENCE; then
	printf "ERROR: cannot find linux-firmware.git at '%s'\n" ${LFWDIR} >&2
	exit 1
fi

################################################################################
#
# Helper functions.
#

get_device_ids_by_flav()
{
	for d in mt7915 mt7921 mt7925 mt7996; do

		case ${d} in
		mt7915)	flav=${d} ;;
		mt7921)	flav=mt792x ;;
		mt7925)	flav=mt792x ;;
		mt7996)	flav=${d} ;;
		*)	printf "ERROR: unsupported directory/flavor '%s'\n" ${d} >&2
			exit 1
			;;
		esac

		awk -v flav=${flav} -v rege="/${d}_pci_device_table/" 'BEGIN { x = 0; } {
			if (rege) { x=1 };
			if (/^\};/) { x=0 };
			if (x==1 && /PCI_DEVICE\(PCI_VENDOR_ID_MEDIATEK,/) {
				gsub(").*", "", $3);
				#printf "%s)\taddpkg \"wifi-firmware-mt76-kmod-%s\"; return 1 ;;\n", tolower($3), tolower(flav);
				printf "%s\t%s\n", tolower(flav), tolower($3);
			}
		}' ${d}/pci.c

		#grep -1r 'PCI_DEVICE(PCI_VENDOR_ID_MEDIATEK' ${flav}
	done
}

get_firmwares_by_flavor()
{
	for h in mt7915/mt7915.h mt792x.h mt7996/mt7996.h; do

		case ${h} in
		mt7915/mt7915.h)	flav=mt7915 ;;
		mt792x.h)		flav=mt792x ;;
		mt7996/mt7996.h)	flav=mt7996 ;;
		*)	printf "ERROR: unsupported header/flavor '%s'\n" ${h} >&2
			exit 1
			;;
		esac

		awk -v rege="${FWSUBDIR}/" -v flav=${flav} -F\" '{ if ($0 ~ rege) { printf "%s\t%s\n", tolower(flav), $2; } }' ${h} | \
		while read flav fwn; do
			if test ! -e ${LFWDIR}/${fwn}; then
				#printf "Firmware %s (for %s) does not exist; skipping\n" ${fwn} ${flav} >&2
				continue
			fi
			printf "%s\t%s\n" ${flav} ${fwn}
		done
	done
}

list_fw()
{
	# List of already seen flavor (firmware directory).
	sfwl=""

	# List of "supported" device IDs.
	devidl=`get_device_ids_by_flav`
	#echo "===> ${devidl}" >&2

	# List of (flavor) -> (firmware) mappings.
	l=`get_firmwares_by_flavor`
	#echo "===> ${l}" >&2

	# For each flavor check we have at least 1 firmware file or skip it.
	flavs=$(echo "${devidl}" | awk '{ print $1 }' | sort | uniq)
	for flav in ${flavs}; do

		lx=$(echo "${l}" | awk -v flav=${flav} '{ if ($1 ~ flav) { print $2 } }' | sort | uniq)
		fn=$(echo "${lx}" | wc -l | awk '{ print $1 }')
		#printf "=+=> %s -- %s -- %s\n" ${flav} ${lx} ${fn} >&2
		if test ${fn} -le 0; then
			printf "Flavor %s has %s firmware files; skipping\n" ${flav} ${fn} >&2
			continue
		fi

		# Output the PCI ID/flav combinations for this flav.
		echo "${devidl}" | \
		while read _flav did; do
			if test "${_flav}" == "${flav}"; then
				# Print this first or otherwise if two device IDs have the same firmware
				# we may not see that.
				echo "FWGET ${did} ${flav}"
			fi
		done

		x=""
		for zf in ${sfwl}; do
			if test "${zf}" == "${flav}"; then
				x="${zf}"
				break
			fi
		done
		if test "${x}" != ""; then
			printf "Flavor %s already seen; skipping\n" ${flav} >&2
			continue
		fi
		sfwl="${sfwl} ${flav}"

		# Handle ports bits.
		echo "FWS ${flav}"
		echo "DISTFILES_${flav}= \\"
		for fz in ${lx}; do echo "${fz}"; done | \
		awk -v fn=$fn -v fwg=${flav} -v drv=${FWSUBDIR} '{
			if (FNR == fn) { x="" } else { x=" \\" };
			gsub("^" drv "/", "${FWSUBDIR}/");
			#gsub("^", "${FWSUBDIR}/");
			printf "\t%s${DISTURL_SUFFIX}%s\n", $0, x;
		}'

		# Check for "lic" files.
		# Known known to us, so no idea how to check for them.
	done
}

################################################################################
#
# Generate the PORTS file template.
#

fwsl=$(list_fw | grep ^FWS | awk '{ print $2 }')
# Get a count so we can automatically add \\ apart from the last line.
fn=$(echo "${fwsl}" | wc -w | awk '{ print $1 }')

if test ${fn} -gt 0; then

	portsfile=$(mktemp -p /tmp ${DRIVER}-fwport.XXXXXX)

	:> ${portsfile}
	(
	echo "FWSUBS= \\"
	for sz in ${fwsl}; do echo "${sz}"; done | \
	awk -v fn=$fn '{
		if (FNR == fn) { x="" } else { x=" \\" };
		printf "\t%s%s\n", $0, x;
	}'

	echo
	list_fw | grep -v ^FWS | grep -v ^FWL | grep -v ^FWGET

	echo
	echo "DISTFILES_\${FWDRV}= \\"
	for sz in ${fwsl}; do echo "${sz}"; done | \
	awk -v fn=$fn '{
		if (FNR == fn) { x="" } else { x=" \\" };
		printf "\t${DISTFILES_%s}%s\n", $0, x;
	}'

	fwsl=$(list_fw | grep ^FWL | awk '{ print $2 }')
	# Get a count so we can automatically add \\ apart from the last line.
	fn=$(echo "${fwsl}" | wc -w | awk '{ print $1 }')
	if test ${fn} -gt 0; then
		echo "DISTFILES_\${FWDRV}_lic= \\"
		for sz in ${fwsl}; do echo "${sz}"; done | \
		awk -v fn=$fn '{
			if (FNR == fn) { x="" } else { x=" \\" };
			printf "\t${DISTFILES_%s_lic}%s\n", $0, x;
		}'
	else
		echo "DISTFILES_\${FWDRV}_lic="
	fi

	) >> ${portsfile}

	printf "INFO: wifi-firmware-%s-kmod template at %s\n" ${DRIVER} ${portsfile} >&2
fi

################################################################################
#
# Generate the fwget(8) case pattern table (PCI device ID -> fw port flavor).
#

fwgetfile=$(mktemp -p /tmp ${DRIVER}-fwget.XXXXXX)
:> ${fwgetfile}

fwsl=$(list_fw | grep ^FWGET | sort)
# Get a count so we can automatically add \\ apart from the last line.
fn=$(echo "${fwsl}" | grep -c FWGET | awk '{ print $1 }')

if test ${fn} -gt 0; then

	# We need to check for same ID with multiple firmware.
	# The list ist sorted by ID so duplicates are next to each other.
	cs=$(echo "${fwsl}" | awk '{ print $2 }' | uniq -c | awk '{ print $1 }')

	#echo "==> cs=${cs}" >&2

	for n in ${cs}; do

		# Skip FWGET
		fwsl=${fwsl#*[[:space:]]}
		# get device ID
		did=${fwsl%%[[:space:]]*}
		fwsl=${fwsl#*[[:space:]]}
		# get flavor
		flav=${fwsl%%[[:space:]]*}
		fwsl=${fwsl#*[[:space:]]}

		# printf "===> did %s flav %s\n" ${did} ${flav} >&2

		if test ${n} -eq 1; then
			echo "${did} ${flav}" | awk -v drv=${FWSUBDIR} '{
				printf "\t%s)\taddpkg \"wifi-firmware-%s-kmod-%s\"; return 1 ;;\n",
				    tolower($1), drv, tolower($2);
			}' >> ${fwgetfile}
		else
			echo "${did} ${flav}" | awk -v drv=${FWSUBDIR} '{
				printf "\t%s)\taddpkg \"wifi-firmware-%s-kmod-%s\"\n",
				    tolower($1), drv, tolower($2);
			}' >> ${fwgetfile}

			i=1
			while test ${i} -lt ${n}; do
				# Skip FWGET
				fwsl=${fwsl#*[[:space:]]}
				# get device ID
				did=${fwsl%%[[:space:]]*}
				fwsl=${fwsl#*[[:space:]]}
				# get flavor
				flav=${fwsl%%[[:space:]]*}
				fwsl=${fwsl#*[[:space:]]}

				#printf "===> did %s flav %s\n" ${did} ${flav} >&2

				echo "${did} ${flav}" | awk -v drv=${FWSUBDIR} '{
					printf "\t\taddpkg \"wifi-firmware-%s-kmod-%s\"\n",
					    drv, tolower($2);
				}' >> ${fwgetfile}

				i=$((i + 1))
			done

			printf "\t\treturn 1 ;;\n" >> ${fwgetfile}
		fi
	done
fi

printf "INFO: fwget pci_network_qca %s template at %s\n" ${DRIVER} ${fwgetfile} >&2

# end
