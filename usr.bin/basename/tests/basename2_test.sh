#
# Copyright (c) 2026 Christos Polimatidis
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case empty_operands

empty_operands_head()
{
	atf_set "descr" "Check handling of empty operands"
}

empty_operands_body()
{
	atf_check -s exit:0 -o inline:"\n" -e empty \
	    basename ""

	atf_check -s exit:0 -o inline:"\nsample.txt\n" -e empty \
	    basename -a "" sample.txt

	atf_check -s exit:0 -o inline:"sample.txt\n\n" -e empty \
	    basename -a sample.txt ""
}

atf_init_test_cases()
{
	atf_add_test_case empty_operands
}
