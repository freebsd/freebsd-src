#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024-2026 The FreeBSD Foundation
#
# This software was developed by Björn Zeeb
# under sponsorship from the FreeBSD Foundation.
#
# This is neither efficient nor elegant but we need it few times
# a year and it does the job.  And it turns out there is a lot of
# churn so keep the solutions quick and dirty.
#

set -e

unsortedf=$(mktemp -p /tmp iwlwifi-fwget-unsorted.XXXXXX)
sortedf=$(mktemp -p /tmp iwlwifi-fwget-sorted.XXXXXX)
:> ${unsortedf}
:> ${sortedf}

cpp -DCONFIG_IWLMVM -DCONFIG_IWLMLD \
    -I../../../compat/linuxkpi/common/include pcie/drv.c 2> /dev/null | \
grep __builtin_choose_expr | \
awk 'BEGIN {
	lt=""
}
{
	if (!/.vendor = 0x8086,/) {
		printf "ERROR: %s\n", $0;
		exit
	}

	d=$6;
	sv=$9;
	sd=$12;
	t=$15;


	gsub("\\(", "", d);
	gsub(")", "", d);
	gsub(",", "", d);

	gsub("\\(", "", sv);
	gsub(")", "", sv);
	gsub(",", "", sv);

	gsub("\\(", "", sd);
	gsub(")", "", sd);
	gsub(",", "", sd);

	gsub("-1U", "*", d);
	gsub("-1U", "*", sv);
	gsub("-1U", "*", sd);

	gsub("^.*\\(", "", t);
	gsub(")", "", t);
	gsub(",", "", t);
	gsub("^iwl_", "", t);
	gsub("^iwl", "", t);
	gsub("_mac_cfg$", "", t);
	gsub("_.*$", "", t);

	t=tolower(t)
	if (t != lt) {
		printf "\n\t# %s\n", t;
		lt = t;
	}
	if (t == "9560") { t = "9000"; }
	if (t == "ax200") { t = "22000"; }
	if (t == "qu") { t = "22000"; }
	if (t == "cc") { t = "22000"; }
	if (t == "ma") { t = "ax210"; }
	if (t == "so") { t = "ax210"; }
	if (t == "ty") { t = "ax210"; }
	if (t == "gl") { t = "bz"; }
	if (t != "7000" && t != "8000" && t != "9000" && t != "22000" && t != "ax210" && t != "bz" && t != "sc") {
		printf "ERROR: invalid flavor '%s': %s\n", t, $0;
		exit
	}

	printf "\t%s/%s/%s) addpkg \"wifi-firmware-iwlwifi-kmod-%s\"; return 1 ;;\n", tolower(d), tolower(sv), tolower(sd), t;

}' > ${unsortedf}

( for f in \
        7000 \
        8000 \
        9000 \
        22000 \
        ax210 \
        bz \
        sc ; do

	printf "\n\t# %s\n" ${f}
	grep "wifi-firmware-iwlwifi-kmod-${f}" ${unsortedf} | sort -n

done; echo ) > ${sortedf}

rm -f ${unsortedf}
printf "INFO: fwget(8) template at %s\n" ${sortedf} >&2

# end
