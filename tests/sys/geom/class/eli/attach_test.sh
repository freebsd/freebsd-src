# $FreeBSD$

atf_test_case attach_d cleanup
attach_d_head()
{
	atf_set "descr" "geli attach -d will cause the provider to detach on last close"
	atf_set "require.user" "root"
}
attach_d_body()
{
	. $(atf_get_srcdir)/conf.sh

	sectors=100
	md=$(attach_md -t malloc -s `expr $sectors + 1`)

	atf_check dd if=/dev/random of=keyfile bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile ${md}
	atf_check geli attach -d -p -k keyfile ${md}

	# Be sure it doesn't detach on read.
	atf_check dd if=/dev/${md}.eli of=/dev/null status=none
	sleep 1
	if [ ! -c /dev/${md}.eli ]; then
		atf_fail "Detached on last close of a reader"
	fi

	# It should detach on last close of a writer
	true > /dev/${md}.eli
	sleep 1
	if [ -c /dev/${md}.eli ]; then
		atf_fail "Did not detach on last close of a writer"
	fi

}
attach_d_cleanup()
{
	. $(atf_get_srcdir)/conf.sh
	geli_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case attach_d
}
