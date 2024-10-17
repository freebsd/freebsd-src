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
# USAGE: please make sure to pre-load if_iwlwifi.ko so that we
# have access to the sysctl.  You do not need to have a supported
# card in the system.
# In case that is not possible you can save the results to a file
# and provide that locally.  It will be renamed at the end of the
# run.
#

set -e

# sysctl -n compat.linuxkpi.iwlwifi_pci_ids_name > iwlwifi_pci_ids_name.txt
PCI_IDS_FILE=iwlwifi_pci_ids_name.txt
D_PCI_IDS_FILE=`pwd`/${PCI_IDS_FILE}

################################################################################
#
# Check pre-reqs
#
if [ $# -ne 1 ]; then
	printf "USAGE: %s /path/to/linux-firmware.git\n" $0 >&2
	exit 1
fi

if [ ! -d cfg/ -o ! -e cfg/bz.c ]; then
	printf "ERROR: run from iwlwifi driver directory; no cfg/bz.c here\n" >&2
	exit 1
fi

LFWDIR=${1}
if test ! -d ${LFWDIR} -o ! -e ${LFWDIR}/WHENCE; then
	printf "ERROR: cannot find linux-firmware.git at '%s'\n" ${LFWDIR} >&2
	exit 1
fi

kldstat -n if_iwlwifi.ko > /dev/null 2>&1
rc=$?
case ${rc} in
0)	;;
*)	printf "ERROR: please pre-load if_iwlwifi.ko (you do not need a device)\n" >&2
	exit 1
	;;
esac

if test -r ${D_PCI_IDS_FILE}; then
	printf "NOTICE: using proovided ${D_PCI_IDS_FILE}\n" >&2
else
	sysctl -N compat.linuxkpi.iwlwifi_pci_ids_name > /dev/null 2>&1
	rc=$?
	case ${rc} in
	0)	sysctl -n compat.linuxkpi.iwlwifi_pci_ids_name > ${D_PCI_IDS_FILE}
		;;
	*)	printf "ERROR: cannot get compat.linuxkpi.iwlwifi_pci_ids_name\n" >&2
		exit 1
		;;
	esac
fi

# We need to be in the config directory for simplicity.
cd cfg

################################################################################

# Get a list of all device/firmware flavors as seen/supported by the driver.
flavors=$(awk -F\\t '{
	if (/^$/) { next; }
	if ($5 == "undefined") { next; }
	print tolower($5);
}' ${D_PCI_IDS_FILE} | sort -V | uniq)

################################################################################
#
# Helper functions.
#

#
# This uses a hack (cpp) to expand some macros for us and parses out the result
# which is the firmware name with the maximum FW version supported for that
# firmware.
# We then go and check that said firmware actually exists in linux-firmware.git.
# We try to find a lower version number if the "MAX" version given from the cpp
# output does not (yet) publicly exist.
# .pnvm files are now properly listed as MODULE_FIRMWARE so no more magic needed
# for them.
# Given the filename matches a "flavor" at this point, we then group all the
# available firmware files from this flavor together and print it as a ports
# Makefile variable.
#
# We also print some other meta-data that callers will filter out depending on
# their needs to generate other lists and mappings.
#

# For each get a list of firmware names we know.
list_fw()
{
	for f in ${flavors}; do
		#echo "==> ${f}"
		#awk -F \\t -v flav=${f} '{
		#	if ($5 != flav) { next; }
		#	# No firmwre; skip.
		#	if ($3 ~ /^$/) { next; }
		#	if ($3 == "(null)") { next; };
		#	print $3;
		#}' ${D_PCI_IDS_FILE} | sort | uniq

		# For now the flavor names and the file names are 1:1 which makes this
		# a lot easier (given some sysctl/file entries are not able to list
		# their firmware but we know their "flavor".
		l=$(cpp ${f}.c 2>&1 | awk '
			/^MODULE_FIRMWARE\(/ {
				gsub(/"/, "");
				gsub("__stringify\\(", "");
				gsub("\\);$", "");
				gsub("\\)", "");
				gsub("^MODULE_FIRMWARE\\(", "");
				gsub(" ", "");
				printf "%s\n", $0;
		}' | sort -V | uniq)
		#echo "${l}"

		lx=""
		for fx in ${l}; do
			if test -e ${LFWDIR}/${fx}; then
				lx="${lx} ${fx}"

				# Check for matching .pnvm file.
				# They are now properly listed in MODULE_FIRMWARE() as well so no more magic.
				#px=$(echo ${fx} | awk '{ gsub("-[[:digit:]]*.ucode", ".pnvm"); print; }')
				#if test -e ${LFWDIR}/${px}; then
				#	lx="${lx} ${px}"
				#fi
			else
				case "${fx}" in
				*.pnvm)
					printf "NOTICE: pnvm file not found for '%s'\n" ${fx} >&2
					;;
				*.ucode)
					# Try lowering the version number.
					bn=$(echo ${fx} | awk '{ gsub("-[[:digit:]]*.ucode", ""); print; }')
					vn=$(echo ${fx} | awk '{ gsub(".ucode$", ""); gsub("^.*-", ""); print; }')
					#echo "BN ${bn} VN ${vn}"
					# Single digits are not zero-padded so just ${i} will be fine.
					for i in `jot ${vn} 1`; do
						xn="${bn}-${i}.ucode"
						if test -e ${LFWDIR}/${xn}; then
							lx="${lx} ${xn}"
							break 2;
						fi
					done
					;;
				*)
					printf "NOTICE: file for unknown firmware type not found for '%s'\n" ${fx} >&2
					;;
				esac
			fi
		done

		# Get a count so we can automatically add \\ apart from the last line.
		fn=$(echo "${lx}" | wc -w | awk '{ print $1 }')

		#echo "==> ${f} :: ${fn} :: ${lx}"

		if test ${fn} -gt 0; then

			# Ports FLAVOR names are [a-z0-9_].  If needed add more mangling magic here.
			flav=`echo ${f} | awk '{ printf "%s", tolower($0); }'`

			echo "FWS ${flav}"
			echo "DISTFILES_${flav}= \\"
			for fz in ${lx}; do echo "${fz}"; done | \
			awk -v fn=$fn -v fwg=${flav} '{
				if (FNR == fn) { x="" } else { x=" \\" };
				printf "\t%s${DISTURL_SUFFIX}%s\n", $0, x;
				fwn=$0;
				gsub("-[[:digit:]]*\.ucode$", "", fwn);
				printf "FWGET %s %s\n", fwg, fwn;
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

	portsfile=$(mktemp -p /tmp iwlwifi-fwport.XXXXXX)

	:> ${portsfile}
	(
	echo "FWSUBS= \\"
	for sz in ${fwsl}; do echo "${sz}"; done | \
	awk -v fn=$fn '{ if (FNR == fn) { x="" } else { x=" \\" }; printf "\t%s%s\n", $0, x; }'

	echo
	echo "# Do not prefix with empty \${FWSUBDIR}/!"
	list_fw | grep -v ^FWS | grep -v ^FWGET

	echo
	echo "DISTFILES_\${FWDRV}= \\"
	for sz in ${fwsl}; do echo "${sz}"; done | \
	awk -v fn=$fn '{ if (FNR == fn) { x="" } else { x=" \\" }; printf "\t${DISTFILES_%s}%s\n", $0, x; }'
	echo "DISTFILES_\${FWDRV}_lic="
	) >> ${portsfile}

	printf "INFO: wifi-firmware-iwlwifi-kmod template at %s\n" ${portsfile} >&2
fi

################################################################################
#
# Generate a temporary firmware -> flavor mapping table for fwget generation.
#

mapfile=$(mktemp -p /tmp iwlwifi-mapfile.XXXXXX)
:> ${mapfile}

fwgl=$(list_fw | grep FWGET)
# Get a count so we can automatically add \\ apart from the last line.
fn=$(echo "${fwgl}" | wc -w | awk '{ print $1 }')
if test ${fn} -gt 0; then

	(
	list_fw | grep FWGET | grep -v '.pnvm' | \
	while read x flav fw; do
		printf "%s\t%s\n" ${fw} ${flav}
	done | \
	sort -n | uniq
	) >> ${mapfile}
fi

################################################################################
#
# Try to generate the PCI ID -> port flavor mapping
#
# We get PCI ID, description, firmware base from the sysctl and can work our
# way back from fw name base to flavor via the mapping table file.
#

fwgetfile=$(mktemp -p /tmp iwlwifi-fwget.XXXXXX)
:> ${fwgetfile}

awk 'BEGIN { FS="\t"; }
{
	# Skip empty lines.
	if (/^$/) { next; }
	# Skip "undefined" flavors as we have no idea what chipset.
	if ($5 == "undefined") { next; }

	# No firmware name; do not skip!
	# All we need is the flavor, which we now always have.
	#if ($3 == "(null)") { next; };

	FLAV=tolower($5);

	split($1, i, "/");
	gsub("\t.*$", "", i[4]);

	# Not an Intel Vednor ID; skip.
	if (i[1] != "0x8086") { next; };

	# No defined device ID; skip.
	if (i[2] == "0xffff") { next; };

	# Adjust wildcards or a ill-printed 0.
	if (i[3] == "0xffffffff") { i[3] = "*"; };
	if (i[4] == "000000") { i[4] = "0x0000"; };
	if (i[4] == "0xffffffff") { i[4] = "*"; };
	if (i[4] == "0xffff") { i[4] = "*"; };

	printf "%s\t%s/%s/%s\n", FLAV, i[2], i[3], i[4];
}' ${D_PCI_IDS_FILE} | \
sort -V | uniq | \
while read flav match; do

	#flav=$(awk -v fw=$fw '{ if ($1 == fw) { print $2; } }' ${mapfile})
	#echo "${fw} :: ${match} :: ${flav}"

	if test "${flav}" != ""; then
		printf "${flav}\t${match}\t${flav}\n"
	else
		#echo "NO FLAV ${fw} ${match}" >&2
	fi

done | \
awk 'BEGIN { FS="\t"; FWN=""; }
{
	FW=$1;
	if (FWN != FW) { printf "\n\t# %s\n", FW; FWN=FW; };

	printf "\t%s) addpkg \"wifi-firmware-iwlwifi-kmod-%s\"; return 1 ;;\n", $2, $3;
} END {
	printf "\n";
}' >> ${fwgetfile}

printf "INFO: fwget pci_network_intel template at %s\n" ${fwgetfile} >&2

################################################################################
#
# Try to build the iwlwififw.4 bits too.
#

dl=$(grep -v ^$ ${D_PCI_IDS_FILE} | uniq | \
awk '
{
	# Sourt out duplicate lines.
	if (dup[$0]++) { next; }

	# my ($ids, $name, $fw) = split /\t/;
	split($0, a, "\t");
	ids=a[1];
	name=a[2];
	fw=a[3];

	#my ($v, $d, $sv, $sd) = split("/", $ids);
	split(ids, i, "/");
	gsub("^0xffff+", "any", i[1]);
	gsub("^0xffff+", "any", i[2]);
	gsub("^0xffff+", "any", i[3]);
	gsub("^0xffff+", "any", i[4]);

	if (name == "") { name="(unknown)"; }
	if (fw == "") { fw="(unknown)"; }

	# iwlwififw.4
	printf ".It \"\"\n.It %s\n.It %s Ta %s Ta %s Ta %s Ta %s\n", name, i[1], i[2], i[3], i[4], fw;

	# wiki
	# XXX TODO possibly quote some in `` to avoid automatic linking?
	# || PCI IDs || Chipset Name || Firmware prefix || Comment ||
	printf "WIKI || %s / %s / %s / %s || %s || %s || ||\n", i[1], i[2], i[3], i[4], name, fw;
	if ((FNR % 25) == 0) { printf "WIKI \n"; }
}')

manfwfile=$(mktemp -p /tmp iwlwifi-iwlwififw4.XXXXXX)
:> ${manfwfile}
echo "${dl}" | grep -v ^WIKI >> ${manfwfile}
printf "INFO: share/man/man4/iwlwififw.4 template at %s\n" ${manfwfile} >&2

wikifile=$(mktemp -p /tmp iwlwifi-wiki.XXXXXX)
:> ${wikifile}
echo "${dl}" | awk '/^WIKI / { gsub("^WIKI ", ""); print; }' >> ${wikifile}
printf "INFO: WIKI template at %s\n" ${wikifile} >&2


################################################################################
#
# Cleanup
#
rm ${mapfile}
mv -f ${D_PCI_IDS_FILE} ${D_PCI_IDS_FILE}.old

# end
