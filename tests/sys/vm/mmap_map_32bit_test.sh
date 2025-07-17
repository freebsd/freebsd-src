#
# Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Simple test of MAP_32BIT flag w/wo ASLR

map_32bit_w_aslr_head()
{
	atf_set descr "MAP_32BIT with ASLR"
	atf_set require.progs proccontrol
}

map_32bit_w_aslr_body()
{
	atf_check -s exit:0 -x proccontrol -m aslr -s enable \
	    $(atf_get_srcdir)/mmap_map_32bit_helper
}

map_32bit_wo_aslr_head()
{
	atf_set descr "MAP_32BIT without ASLR"
	atf_set require.progs proccontrol
}

map_32bit_wo_aslr_body()
{
	atf_check -s exit:0 -x proccontrol -m aslr -s disable \
	    $(atf_get_srcdir)/mmap_map_32bit_helper
}


atf_init_test_cases()
{
	atf_add_test_case map_32bit_w_aslr
	atf_add_test_case map_32bit_wo_aslr
}
