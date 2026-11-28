# Copyright (c) 2026 Stéphane Rochoy <sheda@FreeBSD.org>
# SPDX-License-Identifier: BSD-2-Clause

atf_test_case basic
cprofile_head()
{
	atf_set "descr" "Cooling profile parsing"
}
cprofile_body()
{
	local EX_CONFIG=78
	local EX_OK=0
	local srcdir=$(atf_get_srcdir)

	# EX_OK
	atf_check -s exit:$EX_OK -o file:$srcdir/celsius.expout -e empty \
	          fand -c $srcdir/celsius.cprofile -l
	atf_check -s exit:$EX_OK -o file:$srcdir/millikelvins.expout -e empty \
	          fand -c $srcdir/millikelvins.cprofile -m 0.001 -K -l

	# EX_CONFIG
	for p in badprop missing-duty missing-loduty missing-steps missing-temp \
	         misordered-steps too-many-steps; do
		atf_check -s exit:$EX_CONFIG -o empty -e file:$srcdir/$p.experr \
		    fand -c $srcdir/$p.cprofile -l
	done
}

atf_init_test_cases()
{
	atf_add_test_case cprofile
}
