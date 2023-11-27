
. $(atf_get_srcdir)/conf.sh

atf_test_case attach_d cleanup
attach_d_head()
{
	atf_set "descr" "geli attach -d will cause the provider to detach on last close"
	atf_set "require.user" "root"
}
attach_d_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`

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
	geli_test_cleanup
}

atf_test_case attach_r cleanup
attach_r_head()
{
	atf_set "descr" "geli attach -r will create a readonly provider"
	atf_set "require.user" "root"
}
attach_r_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`
	atf_check dd if=/dev/random of=keyfile bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile ${md}
	atf_check geli attach -r -p -k keyfile ${md}

	atf_check -o match:"^Flags: .*READ-ONLY" geli list ${md}.eli

	# Verify that writes are verbotten
	atf_check -s not-exit:0 -e match:"Read-only" \
		dd if=/dev/zero of=/dev/${md}.eli count=1
}
attach_r_cleanup()
{
	geli_test_cleanup
}

atf_test_case attach_multiple cleanup
attach_multiple_head()
{
	atf_set "descr" "geli attach can attach multiple providers"
	atf_set "require.user" "root"
}
attach_multiple_body()
{
	geli_test_setup

	sectors=100
	attach_md md0 -t malloc -s `expr $sectors + 1`
	attach_md md1 -t malloc -s `expr $sectors + 1`
	attach_md md2 -t malloc -s `expr $sectors + 1`
	atf_check dd if=/dev/random of=keyfile bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile ${md0}
	atf_check geli init -B none -P -K keyfile ${md1}
	atf_check geli init -B none -P -K keyfile ${md2}
	atf_check geli attach -p -k keyfile ${md0} ${md1} ${md2}
	# verify that it did create all 3 geli devices
	atf_check -s exit:0 test -c /dev/${md0}.eli
	atf_check -s exit:0 test -c /dev/${md1}.eli
	atf_check -s exit:0 test -c /dev/${md2}.eli
}
attach_multiple_cleanup()
{
	geli_test_cleanup
}

atf_test_case nokey cleanup
nokey_head()
{
	atf_set "descr" "geli attach fails if called with no key component"
	atf_set "require.user" "root"
}
nokey_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`
	atf_check dd if=/dev/random of=keyfile bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile ${md}
	atf_check -s not-exit:0 -e match:"No key components given" \
		geli attach -p ${md} 2>/dev/null
}
nokey_cleanup()
{
	geli_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case attach_d
	atf_add_test_case attach_r
	atf_add_test_case attach_multiple
	atf_add_test_case nokey
}
