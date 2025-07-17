#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Christos Margiolis <christos@FreeBSD.org>
#

get_months_fmt()
{
	rm -f in
        for i in $(seq 12 1); do
                printf "2000-%02d-01\n" ${i} | xargs -I{} \
                date -jf "%Y-%m-%d" {} "${1}" >>in
        done
}

atf_test_case monthsort_english
monthsort_english_head()
{
	atf_set "descr" "Test the -M flag with English months"
}
monthsort_english_body()
{
	export LC_TIME="en_US.UTF-8"

	cat >expout <<EOF
January
February
March
April
May
June
July
August
September
October
November
December
EOF

	# No need to test the rest of the formats (%b and %OB) as %b is a
	# substring of %B and %OB is the same as %B.
	get_months_fmt '+%B'
	atf_check -o file:expout sort -M in
}

atf_test_case monthsort_all_formats_greek
monthsort_all_formats_greek_head()
{
	atf_set "descr" "Test the -M flag with all possible Greek month formats"
}
monthsort_all_formats_greek_body()
{
	# Test with the Greek locale, since, unlike English, the
	# abbreviation/full-name and standalone formats are different.
	export LC_TIME="el_GR.UTF-8"

	# Abbreviation format (e.g Jan, Ιαν)
	cat >expout <<EOF
Ιαν
Φεβ
Μαρ
Απρ
Μαΐ
Ιουν
Ιουλ
Αυγ
Σεπ
Οκτ
Νοε
Δεκ
EOF
	get_months_fmt '+%b'
	atf_check -o file:expout sort -M in

	# Full-name format (e.g January, Ιανουαρίου)
	cat >expout <<EOF
Ιανουαρίου
Φεβρουαρίου
Μαρτίου
Απριλίου
Μαΐου
Ιουνίου
Ιουλίου
Αυγούστου
Σεπτεμβρίου
Οκτωβρίου
Νοεμβρίου
Δεκεμβρίου
EOF
	get_months_fmt '+%B'
	atf_check -o file:expout sort -M in

	# Standalone format (e.g January, Ιανουάριος)
	cat >expout <<EOF
Ιανουάριος
Φεβρουάριος
Μάρτιος
Απρίλιος
Μάϊος
Ιούνιος
Ιούλιος
Αύγουστος
Σεπτέμβριος
Οκτώβριος
Νοέμβριος
Δεκέμβριος
EOF
	get_months_fmt '+%OB'
	atf_check -o file:expout sort -M in
}

atf_test_case monthsort_mixed_formats_greek
monthsort_mixed_formats_greek_head()
{
	atf_set "descr" "Test the -M flag with mixed Greek month formats"
}
monthsort_mixed_formats_greek_body()
{
	export LC_TIME="el_GR.UTF-8"

	cat >in <<EOF
Δεκέμβριος
Νοεμβρίου
Οκτ
Σεπ
Αυγ
Ιούλιος
Ιουνίου
Μαΐου
Απριλίου
Μάρτιος
Φεβρουάριος
Ιανουάριος
EOF

	cat >expout <<EOF
Ιανουάριος
Φεβρουάριος
Μάρτιος
Απριλίου
Μαΐου
Ιουνίου
Ιούλιος
Αυγ
Σεπ
Οκτ
Νοεμβρίου
Δεκέμβριος
EOF

	atf_check -o file:expout sort -M in
}

atf_init_test_cases()
{
	atf_add_test_case monthsort_english
	atf_add_test_case monthsort_all_formats_greek
	atf_add_test_case monthsort_mixed_formats_greek
}
