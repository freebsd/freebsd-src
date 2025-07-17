#!/bin/sh

. $(atf_get_srcdir)/conf.sh

atf_test_case setkey cleanup
setkey_head()
{
	atf_set "descr" "geli setkey can change the key for an existing provider"
	atf_set "require.user" "root"
}
setkey_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`

	atf_check dd if=/dev/random of=rnd bs=512 count=${sectors} status=none
	hash1=`dd if=rnd bs=512 count=${sectors} status=none | md5`
	atf_check_equal 0 $?
	atf_check dd if=/dev/random of=keyfile1 bs=512 count=16 status=none
	atf_check dd if=/dev/random of=keyfile2 bs=512 count=16 status=none
	atf_check dd if=/dev/random of=keyfile3 bs=512 count=16 status=none
	atf_check dd if=/dev/random of=keyfile4 bs=512 count=16 status=none
	atf_check dd if=/dev/random of=keyfile5 bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile1 ${md}
	atf_check geli attach -p -k keyfile1 ${md}

	atf_check \
		dd if=rnd of=/dev/${md}.eli bs=512 count=${sectors} status=none
	hash2=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check_equal 0 $?

	# Change current key (0) for attached provider.
	atf_check -s exit:0 -o ignore geli setkey -P -K keyfile2 ${md}
	atf_check geli detach ${md}

	# We cannot use keyfile1 anymore.
	atf_check -s not-exit:0 -e match:"Wrong key" \
		geli attach -p -k keyfile1 ${md}

	# Attach with new key.
	atf_check geli attach -p -k keyfile2 ${md}
	hash3=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check_equal 0 $?

	# Change key 1 for attached provider.
	atf_check -s exit:0 -o ignore geli setkey -n 1 -P -K keyfile3 ${md}
	atf_check geli detach ${md}

	# Attach with key 1.
	atf_check geli attach -p -k keyfile3 ${md}
	hash4=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check_equal 0 $?
	atf_check geli detach ${md}

	# Change current (1) key for detached provider.
	atf_check -s exit:0 -o ignore geli setkey -p -k keyfile3 -P -K keyfile4 ${md}

	# We cannot use keyfile3 anymore.
	atf_check -s not-exit:0 -e match:"Wrong key" \
		geli attach -p -k keyfile3 ${md}

	# Attach with key 1.
	atf_check geli attach -p -k keyfile4 ${md}
	hash5=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check_equal 0 $?
	atf_check geli detach ${md}

	# Change key 0 for detached provider.
	atf_check -s exit:0 -o ignore geli setkey -n 0 -p -k keyfile4 -P -K keyfile5 ${md}

	# We cannot use keyfile2 anymore.
	atf_check -s not-exit:0 -e match:"Wrong key" \
		geli attach -p -k keyfile2 ${md} 2>/dev/null

	# Attach with key 0.
	atf_check geli attach -p -k keyfile5 ${md}
	hash6=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check_equal 0 $?
	atf_check geli detach ${md}

	atf_check_equal ${hash1} ${hash2}
	atf_check_equal ${hash1} ${hash3}
	atf_check_equal ${hash1} ${hash4}
	atf_check_equal ${hash1} ${hash5}
	atf_check_equal ${hash1} ${hash6}
}
setkey_cleanup()
{
	geli_test_cleanup
}

atf_test_case setkey_passphrase cleanup
setkey_passphrase_head()
{
	atf_set "descr" "geli setkey can change the passphrase for a provider"
	atf_set "require.user" "root"
}
setkey_passphrase_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`

	atf_check dd if=/dev/random of=rnd bs=512 count=${sectors} status=none
	hash1=`dd if=rnd bs=512 count=${sectors} status=none | md5`
	atf_check_equal 0 $?
	atf_check dd if=/dev/random of=pass1 bs=512 count=1 status=none
	atf_check dd if=/dev/random of=pass2 bs=512 count=1 status=none
	atf_check dd if=/dev/random of=pass3 bs=512 count=1 status=none

	atf_check geli init -B none -J pass1 ${md}
	atf_check geli attach -j pass1 ${md}

	atf_check \
		dd if=rnd of=/dev/${md}.eli bs=512 count=${sectors} status=none
	hash2=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check_equal 0 $?

	atf_check geli detach ${md}

	# Change from passphrase 1 to passphrase 2 for the detached provider.
	atf_check -s exit:0 -o ignore geli setkey -j pass1 -J pass2 ${md}

	# Make sure that we can attach with passphrase 2 but not with
	# passphrase 1.
	atf_check -s not-exit:0 -e match:"Wrong key" \
		geli attach -j pass1 ${md}
	atf_check -s exit:0 geli attach -j pass2 ${md}
	hash3=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`

	# Change from passphrase 2 to passphrase 3 for the attached provider.
	atf_check -s exit:0 -o ignore geli setkey -j pass2 -J pass3 ${md}
	hash4=`dd if=/dev/${md}.eli bs=512 count=${sectors} 2>/dev/null | md5`
	atf_check geli detach ${md}

        # Make sure that we cannot attach with passphrase 2 anymore.
	atf_check -s not-exit:0 -e match:"Wrong key" \
		geli attach -j pass2 ${md}

	atf_check_equal ${hash1} ${hash2}
	atf_check_equal ${hash1} ${hash3}
	atf_check_equal ${hash1} ${hash4}
}
setkey_passphrase_cleanup()
{
	geli_test_cleanup
}

atf_test_case setkey_readonly cleanup
setkey_readonly_head()
{
	atf_set "descr" "geli setkey cannot change the keys of a readonly provider"
	atf_set "require.user" "root"
}
setkey_readonly_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`
	atf_check dd if=/dev/random of=keyfile bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile ${md}
	atf_check geli attach -r -p -k keyfile ${md}

	atf_check -s not-exit:0 -e match:"read-only" \
		geli setkey -n 1 -P -K /dev/null ${md}
}
setkey_readonly_cleanup()
{
	geli_test_cleanup
}

atf_test_case nokey cleanup
nokey_head()
{
	atf_set "descr" "geli setkey can change the key for an existing provider"
	atf_set "require.user" "root"
}
nokey_body()
{
	geli_test_setup

	sectors=100
	attach_md md -t malloc -s `expr $sectors + 1`
	atf_check dd if=/dev/random of=keyfile1 bs=512 count=16 status=none
	atf_check dd if=/dev/random of=keyfile2 bs=512 count=16 status=none

	atf_check geli init -B none -P -K keyfile1 ${md}

	# Try to set the key for a detached device without providing any
	# components for the old key.
	atf_check -s not-exit:0 -e match:"No key components given" \
		geli setkey -n 0 -p -P -K keyfile2 ${md}

	# Try to set the key for a detached device without providing any
	# components for the new key
	atf_check -s not-exit:0 -e match:"No key components given" \
		geli setkey -n 0 -p -k keyfile1 -P ${md}

	# Try to set a new key for an attached device with no components
	atf_check geli attach -p -k keyfile1 ${md}
	atf_check -s not-exit:0 -e match:"No key components given" \
		geli setkey -n 0 -P ${md}
}
nokey_cleanup()
{
	geli_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case setkey
	atf_add_test_case setkey_passphrase
	atf_add_test_case setkey_readonly
	atf_add_test_case nokey
}
