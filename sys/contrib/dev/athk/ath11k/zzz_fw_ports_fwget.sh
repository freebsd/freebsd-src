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

DRIVER=ath11k
CHECKFILE=debugfs_htt_stats.c

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
LFWDIR=${LFWDIR}/${DRIVER}

################################################################################
#
# Helper functions.
#
# This uses a hack (cpp) to expand some macros for us and parses out the result
# which is the PCI device ID or the firmware directory for that.
#
# Driver is there, the firmware not yet...?
# ==> 0x1101 -> ATH11K_HW_QCA6390_HW20 -> QCA6390/hw2.0
# ==> 0x1104 -> ATH11K_HW_QCN9074_HW10 -> QCN9074/hw1.0
# ==> 0x1103 -> ATH11K_HW_WCN6855_HW20 -> WCN6855/hw2.0
# ==> 0x1103 -> ATH11K_HW_WCN6855_HW21 -> WCN6855/hw2.1
# Firmware dir WCN6855/hw2.1 (for 0x1103) does not exist; skipping
#
list_fw()
{
	# List of already seen flavor (firmware directory).
	sfwl=""

	# List of "supported" device IDs.
	devidl=$(cpp pci.c 2> /dev/null | awk '/PCI_VDEVICE\(QCOM,/ { gsub("^.*, ", ""); gsub("\\) },$", ""); print tolower($0); }')
	# Turn them into a regex.
	didreg=$(echo "${devidl}" | xargs -J % echo -n % | sed -e 's, ,|,g')
	# List the device ID cases along with their hw_rev which we can go and take to lookup fw.
	hwrevs=$(cpp pci.c 2> /dev/null | egrep -E "(case (${didreg})|ab->hw_rev = )" | awk '{
		if (FNR > 1 && /case/) {
			printf "\n";
		}
		gsub("^.*case[[:space:]]*", "");
		gsub("[[:space:]]*ab->hw_rev = ", " ");
		gsub("[:;]", "");
		printf "%s", $0;
	}')

	# hwrevs is a list of (device IDs) -> (1..n hardware revisions) mappings.
	#echo "==> ${devidl} :: ${didreg} :: ${hwrevs}" >&2

	# List of (hardware revision) -> (firware directory) mappings.
	l=$(cpp core.c 2> /dev/null | egrep -E '\.(hw_rev|dir) = ' | awk '{ if (/hw_rev/) { printf "%s", $0; } else { print; } }' | sort | uniq | \
	awk '{
		gsub("^.*hw_rev = ", "");
		gsub(",.* = ", "\t");
		gsub(",$", "");
		gsub(/"/, "");
		gsub(" ", "");
		gsub("\t", " ");
		print;
	}')
	#echo "===> ${l}" >&2

	ll=$(echo "${l}" | wc -w | awk '{ print $1 }')
	while test "${ll}" -gt 1; do
		hwr=${l%%[[:space:]]*}
		l=${l#*[[:space:]]}
		fwd=${l%%[[:space:]]*}
		l=${l#*[[:space:]]}

		#echo "=+=> ${hwr} -> ${fwd}" >&2
		eval fwd_${hwr}=${fwd}
		ll=$(echo "${l}" | wc -w | awk '{ print $1 }')
	done

	echo "${hwrevs}" | \
	while read did hwrl; do
		hwrn=$(echo "${hwrl}" | wc -w | awk '{ print $1 }')
		if test ${hwrn} -lt 1; then
			printf "Device ID %s has no hardware revisions (%s); skipping\n" "${did}" ${hwrn} >&2
			continue
		fi

		for hwrx in ${hwrl}; do

			eval fwd=\${fwd_${hwrx}}
			#echo "===> ${did} -> ${hwrx} -> ${fwd}" >&2

			if test ! -d ${LFWDIR}/${fwd}; then
				#printf "Firmware dir %s (for %s) does not exist; skipping\n" ${fwd} ${did} >&2
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
					#gsub("^" drv "/", "${FWSUBDIR}/");
					gsub("^", "${FWSUBDIR}/");
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
						#gsub("^" drv "/", "${FWSUBDIR}/");
						gsub("^", "${FWSUBDIR}/");
						printf "\t%s${DISTURL_SUFFIX}%s\n", $0, x;
					}'
				fi
			fi
		done

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
