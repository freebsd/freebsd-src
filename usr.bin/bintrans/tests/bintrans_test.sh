atf_test_case encode_qp
encode_qp_body()
{
	atf_check -e empty -o file:"$(atf_get_srcdir)/textqpenc" bintrans qp $(atf_get_srcdir)/textqpdec
}

atf_test_case decode_qp
decode_qp_body()
{
	printf "=" > test
	atf_check -e empty -o inline:"=" bintrans qp -u test
	printf "=\ra" > test
	atf_check -e empty -o inline:"=\ra" bintrans qp -u test
	printf "=\r\na" > test
	atf_check -e empty -o inline:"a" bintrans qp -u test
	printf "This is a line" > test
	atf_check -e empty -o inline:"This is a line" bintrans qp -u test
	printf "This= is a line" > test
	atf_check -e empty -o inline:"This= is a line" bintrans qp -u test
	printf "This=2 is a line" > test
	atf_check -e empty -o inline:"This=2 is a line" bintrans qp -u test
	printf "This=23 is a line" > test
	atf_check -e empty -o inline:"This# is a line" bintrans qp -u test
	printf "This=3D is a line" > test
	atf_check -e empty -o inline:"This= is a line" bintrans qp -u test
	printf "This_ is a line" > test
	atf_check -e empty -o inline:"This_ is a line" bintrans qp -u test
	atf_check -e empty -o file:"$(atf_get_srcdir)/textqpdec" bintrans qp -u $(atf_get_srcdir)/textqpenc
}

atf_init_test_cases()
{
	atf_add_test_case decode_qp
	atf_add_test_case encode_qp
}
