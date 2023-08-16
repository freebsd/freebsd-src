
atf_test_case nl

nl_head()
{
	atf_set "descr" "testing just newlines"
}
nl_body()
{
	atf_check \
		-o inline:"a\nb\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/nl.in

	atf_check \
		-o inline:"a\nb\n" \
		-e empty \
		-s exit:0 \
		col -f < $(atf_get_srcdir)/nl.in

	atf_check \
		-o inline:"a\nb\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/nl2.in

	atf_check \
		-o inline:"a\nb\n" \
		-e empty \
		-s exit:0 \
		col -f < $(atf_get_srcdir)/nl2.in

	atf_check \
		-o inline:"a\n\nb\n\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/nl3.in
}

atf_test_case rlf

rlf_head()
{
	atf_set "descr" "testing reverse line feed"
}
rlf_body()
{
	atf_check \
		-o inline:"a b\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/rlf.in

	atf_check \
		-o inline:"a	b\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/rlf2.in

	atf_check \
		-o inline:"a       b\n" \
		-e empty \
		-s exit:0 \
		col -x < $(atf_get_srcdir)/rlf2.in

	atf_check \
		-o inline:" b\na\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/rlf3.in
}

atf_test_case hlf

hlf_head()
{
	atf_set "descr" "testing half line feed"
}
hlf_body()
{
	atf_check \
		-o inline:"a f\naf\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/hlf.in

	atf_check \
		-o inline:"a f9 f9a\n" \
		-e empty \
		-s exit:0 \
		col -f < $(atf_get_srcdir)/hlf.in

	atf_check \
		-o inline:"a\n f\n" \
		-e empty \
		-s exit:0 \
		col < $(atf_get_srcdir)/hlf2.in

	atf_check \
		-o inline:"a9 f\n9"  \
		-e empty \
		-s exit:0 \
		col -f < $(atf_get_srcdir)/hlf2.in
}

atf_init_test_cases()
{
	atf_add_test_case nl
	atf_add_test_case rlf
	atf_add_test_case hlf
}
