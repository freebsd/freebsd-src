
atf_test_case diff3
atf_test_case diff3_lesssimple
atf_test_case diff3_ed
atf_test_case diff3_A
atf_test_case diff3_merge

diff3_body()
{
	atf_check -o file:$(atf_get_srcdir)/1.out \
		diff3 $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/1.out \
		diff3 --strip-trailing-cr $(atf_get_srcdir)/1cr.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/1t.out \
		diff3 -T $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/2.out \
		diff3 -e $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/3.out \
		diff3 -E -L 1 -L 2 -L 3 $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/4.out \
		diff3 -X -L 1 -L 2 -L 3 $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/5.out \
		diff3 -x $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/6.out \
		diff3 -3 $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -o file:$(atf_get_srcdir)/7.out \
		diff3 -i $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt
}

diff3_lesssimple_body()
{
	atf_check -s exit:1 -o file:$(atf_get_srcdir)/10.out \
		diff3 -m -L 1 -L 2 -L 3 $(atf_get_srcdir)/4.txt $(atf_get_srcdir)/5.txt $(atf_get_srcdir)/6.txt
}

diff3_ed_body()
{
	atf_check -s exit:0 -o file:$(atf_get_srcdir)/long-ed.out \
		diff3 -e $(atf_get_srcdir)/long-m.txt $(atf_get_srcdir)/long-o.txt $(atf_get_srcdir)/long-y.txt
}

diff3_A_body()
{
	atf_check -s exit:1 -o file:$(atf_get_srcdir)/8.out \
		diff3 -A -L 1 -L 2 -L 3 $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -s exit:1 -o file:$(atf_get_srcdir)/long-A.out \
		diff3 -A -L long-m.txt -L long-o.txt -L long-y.txt $(atf_get_srcdir)/long-m.txt $(atf_get_srcdir)/long-o.txt $(atf_get_srcdir)/long-y.txt
}


diff3_merge_body()
{
	atf_check -s exit:1 -o file:$(atf_get_srcdir)/9.out \
		diff3 -m -L 1 -L 2 -L 3 $(atf_get_srcdir)/1.txt $(atf_get_srcdir)/2.txt $(atf_get_srcdir)/3.txt

	atf_check -s exit:1 -o file:$(atf_get_srcdir)/long-merge.out \
		diff3 -m -L long-m.txt -L long-o.txt -L long-y.txt $(atf_get_srcdir)/long-m.txt $(atf_get_srcdir)/long-o.txt $(atf_get_srcdir)/long-y.txt
}

atf_init_test_cases()
{
	atf_add_test_case diff3
#	atf_add_test_case diff3_lesssimple
	atf_add_test_case diff3_ed
	atf_add_test_case diff3_A
	atf_add_test_case diff3_merge
}
