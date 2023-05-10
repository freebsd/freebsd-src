#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2019 Yuri Pankov
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

atf_test_case k_flag_posix
k_flag_posix_head()
{
	atf_set "descr" \
	    "Verify output of 'locale -k' for all POSIX specified keywords"
}
k_flag_posix_body()
{
	export LC_ALL="C"

	# LC_MONETARY
	atf_check -o file:"$(atf_get_srcdir)/k_flag_posix_monetary.out" \
	    locale -k \
	    int_curr_symbol \
	    currency_symbol \
	    mon_decimal_point \
	    mon_thousands_sep \
	    mon_grouping \
	    positive_sign \
	    negative_sign \
	    int_frac_digits \
	    frac_digits \
	    p_cs_precedes \
	    p_sep_by_space \
	    n_cs_precedes \
	    n_sep_by_space \
	    p_sign_posn \
	    n_sign_posn \
	    int_p_cs_precedes \
	    int_n_cs_precedes \
	    int_p_sep_by_space \
	    int_n_sep_by_space \
	    int_p_sign_posn \
	    int_n_sign_posn

	# LC_NUMERIC
	atf_check -o file:"$(atf_get_srcdir)/k_flag_posix_numeric.out" \
	    locale -k \
	    decimal_point \
	    thousands_sep \
	    grouping

	# LC_TIME
	atf_check -o file:"$(atf_get_srcdir)/k_flag_posix_time.out" \
	    locale -k \
	    abday \
	    day \
	    abmon \
	    mon \
	    d_t_fmt \
	    d_fmt \
	    t_fmt \
	    am_pm \
	    t_fmt_ampm \
	    era \
	    era_d_fmt \
	    era_t_fmt \
	    era_d_t_fmt \
	    alt_digits

	# LC_MESSAGES
	atf_check -o file:"$(atf_get_srcdir)/k_flag_posix_messages.out" \
	    locale -k \
	    yesexpr \
	    noexpr
}

atf_test_case no_flags_posix
no_flags_posix_head()
{
	atf_set "descr" \
	    "Verify output of 'locale' for all POSIX specified keywords"
}
no_flags_posix_body()
{
	export LC_ALL="C"

	# LC_MONETARY
	atf_check -o file:"$(atf_get_srcdir)/no_flags_posix_monetary.out" \
	    locale \
	    int_curr_symbol \
	    currency_symbol \
	    mon_decimal_point \
	    mon_thousands_sep \
	    mon_grouping \
	    positive_sign \
	    negative_sign \
	    int_frac_digits \
	    frac_digits \
	    p_cs_precedes \
	    p_sep_by_space \
	    n_cs_precedes \
	    n_sep_by_space \
	    p_sign_posn \
	    n_sign_posn \
	    int_p_cs_precedes \
	    int_n_cs_precedes \
	    int_p_sep_by_space \
	    int_n_sep_by_space \
	    int_p_sign_posn \
	    int_n_sign_posn

	# LC_NUMERIC
	atf_check -o file:"$(atf_get_srcdir)/no_flags_posix_numeric.out" \
	    locale \
	    decimal_point \
	    thousands_sep \
	    grouping

	# LC_TIME
	atf_check -o file:"$(atf_get_srcdir)/no_flags_posix_time.out" \
	    locale \
	    abday \
	    day \
	    abmon \
	    mon \
	    d_t_fmt \
	    d_fmt \
	    t_fmt \
	    am_pm \
	    t_fmt_ampm \
	    era \
	    era_d_fmt \
	    era_t_fmt \
	    era_d_t_fmt \
	    alt_digits

	# LC_MESSAGES
	atf_check -o file:"$(atf_get_srcdir)/no_flags_posix_messages.out" \
	    locale \
	    yesexpr \
	    noexpr
}

atf_test_case k_flag_unknown_kw
k_flag_unknown_kw_head()
{
	atf_set "descr" \
	    "Verify 'locale -k' exit status is '1' for unknown keywords"
}
k_flag_unknown_kw_body()
{
	export LC_ALL="C"

	# Hopefully the keyword will stay nonexistent
	atf_check -s exit:1 -o empty -e ignore locale -k nonexistent
}


atf_init_test_cases()
{
	atf_add_test_case k_flag_posix
	atf_add_test_case no_flags_posix
	atf_add_test_case k_flag_unknown_kw
}
