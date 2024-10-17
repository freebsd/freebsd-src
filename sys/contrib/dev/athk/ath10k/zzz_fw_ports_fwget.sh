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

set -e

DRIVER=ath10k
CHECKFILE=qmi_wlfw_v01.c

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
# This uses a hack (cpp) to expand some macros for us and parses out the result
# which is the PCI device ID or the firmware directory for that.
# Checking MODULE_FIRMWARE was pointless as it had too many "dead" entries:
# NOTICE: no firmware file found for 'ath10k/QCA6174/hw2.1/firmware-4.bin'
# NOTICE: no firmware file found for 'ath10k/QCA6174/hw3.0/firmware-5.bin'
# NOTICE: no firmware file found for 'ath10k/QCA9887/hw1.0/board-2.bin'
# NOTICE: no firmware file found for 'ath10k/QCA988X/hw2.0/board-2.bin'
# NOTICE: no firmware file found for 'ath10k/QCA988X/hw2.0/firmware-2.bin'
# NOTICE: no firmware file found for 'ath10k/QCA988X/hw2.0/firmware-3.bin'
#
list_fw()
{
	# List of already seen flavor (firmware directory).
	sfwl=""

	# List of "supported" device IDs (ignoring Ubiquity).
	devidl=$(cpp pci.c 2> /dev/null | awk '/PCI_VDEVICE\(ATHEROS,/ { gsub("^.*, \\(", ""); gsub("\\)\\) },$", ""); print tolower($0); }')

	# List of (device ID) -> (firware directory) mappings.
	cpp core.c 2> /dev/null | egrep -E '\.(dev_id|dir) = ' | awk '{ if (/dev_id/) { printf "%s", $0; } else { print; } }' | grep -v 'dev_id = 0,' | sort | uniq | \
	awk '{
		gsub("^.*\\(", "");
		gsub("),.* = ", "\t");
		gsub(",$", "");
		gsub(/"/, "");
		gsub(" ", "");
		print;
	}' | \
	while read did fwd; do

		x=""
		for d in ${devidl}; do
			if test "${did}" == "${d}"; then
				x="${d}"
				break
			fi
		done
		if test "${x}" == ""; then
			# Device ID not in the list of PCI IDs we support.
			# At least the Ubiquity one we hit here.
			#printf "Device ID %s (%s) not in PCI ID list; skipping\n" ${did} ${fwd} >&2
			continue
		fi

		if test ! -d ${LFWDIR}/${fwd}; then
			# Leave this on as it MUST not happen.
			printf "Firmware dir %s (for %s) does not exist; skipping\n" ${fwd} ${did} >&2
			continue
		fi

		flav=$(echo "${fwd}" | awk -v drv=${DRIVER} '{
			# Ports FLAVOR names are [a-z0-9_].  If needed add more mangling magic here.
			gsub("^" drv "/", "");
			gsub("/", "_");
			gsub("\\.", "");
			print tolower($0);
		}')

		# Print this first or otherwise if two device IDs have the same firmware
		# we may not see that.
		echo "FWGET ${did} ${flav}"

		x=""
		for zf in ${sfwl}; do
			if test "${zf}" == "${flav}"; then
				x="${zf}"
				break
			fi
		done
		if test "${x}" != ""; then
			#printf "Flavor %s (firmware directory %s) already seen; skipping\n" ${flav} ${fwd} >&2
			continue
		fi
		sfwl="${sfwl} ${flav}"

		#echo "==> ${did} -> ${fwd} -> ${flav}"

		lx=$(cd ${LFWDIR} && find ${fwd} -type f \! -name "*sdio*" -a \! -name "*.txt" -print)

		# Get a count so we can automatically add \\ apart from the last line.
		fn=$(echo "${lx}" | wc -w | awk '{ print $1 }')

		#echo "==> ${flav} :: ${fn} :: ${lx}" >&2

		if test ${fn} -gt 0; then

			echo "FWS ${flav}"
			echo "DISTFILES_${flav}= \\"
			for fz in ${lx}; do echo "${fz}"; done | \
			awk -v fn=$fn -v fwg=${flav} -v drv=${DRIVER} '{
				if (FNR == fn) { x="" } else { x=" \\" };
				gsub("^" drv "/", "${FWSUBDIR}/");
				printf "\t%s${DISTURL_SUFFIX}%s\n", $0, x;
			}'

			# Check for "lic" files.
			lx=$(cd ${LFWDIR} && find ${fwd} -type f \! -name "*sdio*" -a -name "*.txt" -print)

			# Get a count so we can automatically add \\ apart from the last line.
			fn=$(echo "${lx}" | wc -w | awk '{ print $1 }')

			if test ${fn} -gt 0; then
				echo "FWL ${flav}"
				echo "DISTFILES_${flav}_lic= \\"
				for fz in ${lx}; do echo "${fz}"; done | \
				awk -v fn=$fn -v fwg=${flav} -v drv=${DRIVER} '{
					if (FNR == fn) { x="" } else { x=" \\" };
					gsub("^" drv "/", "${FWSUBDIR}/");
					printf "\t%s${DISTURL_SUFFIX}%s\n", $0, x;
				}'
			fi
		fi
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
			echo "${did} ${flav}" | awk -v drv=${DRIVER} '{
				printf "\t%s)\taddpkg \"wifi-firmware-%s-kmod-%s\"; return 1 ;;\n",
				    tolower($1), drv, tolower($2);
			}' >> ${fwgetfile}
		else
			echo "${did} ${flav}" | awk -v drv=${DRIVER} '{
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

				echo "${did} ${flav}" | awk -v drv=${DRIVER} '{
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
