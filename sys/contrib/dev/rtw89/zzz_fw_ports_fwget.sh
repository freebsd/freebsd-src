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

DRIVER=rtw89
CHECKFILE=rtw8922a.c

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
# which is the firmware name with the maximum FW version supported for that
# firmware (where applicable).
#
list_fw()
{
	for f in `ls -1 rtw?????.c rtw?????e.c`; do

		l=$(cpp ${f} 2>&1 | awk '/^MODULE_FIRMWARE\(/ { gsub(/"/, ""); gsub("__stringify\\(", ""); gsub("\\);$", ""); gsub("\\)", ""); gsub("^MODULE_FIRMWARE\\(", ""); gsub(" ", ""); printf "%s\n", $0; }' | sort -n | uniq)
		if test "${l}" == ""; then
			continue
		fi
		lx=""
		for fx in ${l}; do
			if test -e ${LFWDIR}/${fx}; then
				lx="${lx} ${fx}"
			# else
			#	printf "NOTICE: no firmware file found for '%s'\n" ${fx} >&2
			fi
		done

		# Get a count so we can automatically add \\ apart from the last line.
		fn=$(echo "${lx}" | wc -w | awk '{ print $1 }')

		# echo "==> ${f} :: ${fn} :: ${lx}"

		if test ${fn} -gt 0; then

			# Ports FLAVOR names are [a-z0-9_].  If needed add more mangling magic here.
			flav=`echo ${f%%.c} | awk '{ printf "%s", tolower($0); }'`

			echo "FWS ${flav}"
			echo "DISTFILES_${flav}= \\"
			for fz in ${lx}; do echo "${fz}"; done | \
			awk -v fn=$fn -v fwg=${flav} -v drv=${DRIVER} '{
				if (FNR == fn) { x="" } else { x=" \\" };
				gsub("^" drv "/", "${FWSUBDIR}/");
				printf "\t%s${DISTURL_SUFFIX}%s\n", $0, x;
			}'
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
	list_fw | grep -v ^FWS

	echo
	echo "DISTFILES_\${FWDRV}= \\"
	for sz in ${fwsl}; do echo "${sz}"; done | \
	awk -v fn=$fn '{
		if (FNR == fn) { x="" } else { x=" \\" };
		printf "\t${DISTFILES_%s}%s\n", $0, x;
	}'
	) >> ${portsfile}

	printf "INFO: wifi-firmware-%s-kmod template at %s\n" ${DRIVER} ${portsfile} >&2
fi

################################################################################
#
# Generate the fwget(8) case pattern table (PCI device ID -> fw port flavor).
#

fwgetfile=$(mktemp -p /tmp ${DRIVER}-fwget.XXXXXX)
:> ${fwgetfile}

for f in `ls -1 rtw?????.c rtw?????e.c`; do

	# Ports FLAVOR names are [a-z0-9_].  If needed add more mangling magic here.
	n=${f%.c};
	n=${n%e};

	awk -v n=${n} -v drv=${DRIVER} '/PCI_DEVICE\(PCI_VENDOR_ID_REALTEK,/ {
		gsub(").*", "", $2);
		printf "\t%s)\taddpkg \"wifi-firmware-%s-kmod-%s\"; return 1 ;;\n",
		    tolower($2), drv, tolower(n);
	}' ${f}
done >> ${fwgetfile}

printf "INFO: fwget pci_network_realtek %s template at %s\n" ${DRIVER} ${fwgetfile} >&2

# end
