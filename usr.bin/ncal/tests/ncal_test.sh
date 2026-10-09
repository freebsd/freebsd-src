#
# SPDX-License-Identifier: BSD-2-Clause
#

export LC_ALL=C

ncal_out()
{
	local name="$1"
	shift

	atf_check -s exit:0 \
	    -o "file:$(atf_get_srcdir)/regress.${name}.out" -e empty \
	    ncal "$@"
}

ncal_err()
{
	local name="$1"
	shift

	atf_check -s exit:64 \
	    -o empty -e "file:$(atf_get_srcdir)/regress.${name}.out" \
	    ncal "$@"
}

atf_test_case year_md
year_md_head()
{
	atf_set "descr" "Full-year calendars in month-day format"
}
year_md_body()
{
	local y

	for y in 2008 2009 2010 2011; do
		ncal_out "r-y${y}-md-nhl" -N -h "${y}"
		ncal_out "b-y${y}-md-nhl" -C -h "${y}"
	done
}

atf_test_case year_jd
year_jd_head()
{
	atf_set "descr" "Full-year calendars in Julian-day format"
}
year_jd_body()
{
	local y

	for y in 2008 2009 2010 2011; do
		ncal_out "r-y${y}-jd-nhl" -N -jh "${y}"
		ncal_out "b-y${y}-jd-nhl" -C -jh "${y}"
	done
}

atf_test_case three_month_md
three_month_md_head()
{
	atf_set "descr" "Three-month calendars in month-day format"
}
three_month_md_body()
{
	local m

	for m in $(jot -w %02d 12); do
		ncal_out "r-3m2009${m}-md-nhl" -N -h3 "${m}" 2009
		ncal_out "b-3m2009${m}-md-nhl" -C -h3 "${m}" 2009
	done
}

atf_test_case three_month_jd
three_month_jd_head()
{
	atf_set "descr" "Three-month calendars in Julian-day format"
}
three_month_jd_body()
{
	local m

	for m in $(jot -w %02d 12); do
		ncal_out "r-3m2009${m}-jd-nhl" -N -jh3 "${m}" 2009
		ncal_out "b-3m2009${m}-jd-nhl" -C -jh3 "${m}" 2009
	done
}

atf_test_case invalid_args
invalid_args_head()
{
	atf_set "descr" "Unsupported option combinations"
}
invalid_args_body()
{
	ncal_err f-3y-nhl -N -3 -y
	ncal_err f-3A-nhl -N -3 -A 3
	ncal_err f-3B-nhl -N -3 -B 3
	ncal_err f-3gy-nhl -N -3 2008
	ncal_err f-3AB-nhl -N -3 -A 3 -B 3
	ncal_err f-mgm-nhl -N -m 3 2 2008
	ncal_err f-ym-nhl -N -y -m 2
	ncal_err f-ygm-nhl -N -y 2 2008
	ncal_err f-yA-nhl -N -y -A 3
	ncal_err f-yB-nhl -N -y -B 3
	ncal_err f-yAB-nhl -N -y -A 3 -B 3
}

atf_test_case cal_options
cal_options_head()
{
	atf_set "descr" "cal-mode -3, -A, -B, and -m combinations"
}
cal_options_body()
{
	ncal_out s-b-3-nhl -C -d 2008.03 -3
	ncal_out s-b-A-nhl -C -d 2008.03 -A 1
	ncal_out s-b-B-nhl -C -d 2008.03 -B 1
	ncal_out s-b-AB-nhl -C -d 2008.03 -A 1 -B 1
	ncal_out s-b-m-nhl -C -d 2008.03 -m 1
	ncal_out s-b-mgy-nhl -C -d 2008.03 -m 1 2007
	ncal_out s-b-gmgy-nhl -C -d 2008.03 1 2007
}

atf_test_case ncal_options
ncal_options_head()
{
	atf_set "descr" "ncal-mode -3, -A, -B, and -m combinations"
}
ncal_options_body()
{
	ncal_out s-r-3-nhl -N -d 2008.03 -3
	ncal_out s-r-A-nhl -N -d 2008.03 -A 1
	ncal_out s-r-B-nhl -N -d 2008.03 -B 1
	ncal_out s-r-AB-nhl -N -d 2008.03 -A 1 -B 1
	ncal_out s-r-m-nhl -N -d 2008.03 -m 1
	ncal_out s-r-mgy-nhl -N -d 2008.03 -m 1 2007
	ncal_out s-r-gmgy-nhl -N -d 2008.03 1 2007
}

atf_test_case sweden
sweden_head()
{
	atf_set "descr" "Sweden's 1700-1712 transitional calendar"
}
sweden_body()
{
	# February 1700 has no leap day and February 1712 has 30 days.
	ncal_out se-r-3-1700-nhl -N -h -3 -s SE -d 1700.02
	ncal_out se-r-3-1712-nhl -N -h -3 -s SE -d 1712.02
}

atf_test_case sweden_jd
sweden_jd_head()
{
	atf_set "descr" "Sweden's transitional calendar in Julian-day format"
}
sweden_jd_body()
{
	ncal_out se-r-jd-170003-nhl -N -h -j -s SE 3 1700
	ncal_out se-r-jd-171202-nhl -N -h -j -s SE 2 1712
}

atf_test_case sweden_julian
sweden_julian_head()
{
	atf_set "descr" "-J overrides the Swedish transitional calendar"
}
sweden_julian_body()
{
	ncal_out se-J-170002-nhl -N -h -s SE -J 2 1700
}

atf_init_test_cases()
{
	atf_add_test_case year_md
	atf_add_test_case year_jd
	atf_add_test_case three_month_md
	atf_add_test_case three_month_jd
	atf_add_test_case invalid_args
	atf_add_test_case cal_options
	atf_add_test_case ncal_options
	atf_add_test_case sweden
	atf_add_test_case sweden_jd
	atf_add_test_case sweden_julian
}
