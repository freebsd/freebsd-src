
mqtest1_head()
{
	:
}
mqtest1_body()
{
	atf_check -s exit:0 -x $(atf_get_srcdir)/mqtest1
}

mqtest2_head()
{
	:
}
mqtest2_body()
{
	atf_check -s exit:0 -x $(atf_get_srcdir)/mqtest2
}

mqtest3_head()
{
	:
}
mqtest3_body()
{
	atf_check -s exit:0 -x $(atf_get_srcdir)/mqtest3
}

mqtest4_head()
{
	:
}
mqtest4_body()
{
	atf_check -s exit:0 -x $(atf_get_srcdir)/mqtest4
}

mqtest5_head()
{
	:
}
mqtest5_body()
{
	atf_check -s exit:0 -x $(atf_get_srcdir)/mqtest5
}

atf_init_test_cases()
{
	atf_add_test_case mqtest1
	atf_add_test_case mqtest2
	atf_add_test_case mqtest3
	atf_add_test_case mqtest4
	atf_add_test_case mqtest5
}
