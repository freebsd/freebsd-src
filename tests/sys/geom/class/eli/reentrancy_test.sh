# $FreeBSD$

# Test various operations for geli-on-geli providers, to ensure that geli is
# reentrant.

. $(atf_get_srcdir)/conf.sh

init_test()
{
	cipher=$1
	aalgo=$2
	secsize=$3
	ealgo=${cipher%%:*}
	keylen=${cipher##*:}

	atf_check dd if=/dev/random of=testdata bs=$secsize count=1 status=none
	atf_check dd if=/dev/random of=keyfile bs=$secsize count=16 status=none

	# Create the lower geli device
	atf_check -s exit:0 -e ignore \
		geli init -B none -a $aalgo -e $ealgo -l $keylen -P -K keyfile \
		-s $secsize ${md}
	atf_check geli attach -p -k keyfile ${md}
	# Create the upper geli device
	atf_check -s exit:0 -e ignore \
		geli init -B none -a $aalgo -e $ealgo -l $keylen -P -K keyfile \
		-s $secsize ${md}.eli
	atf_check geli attach -p -k keyfile ${md}.eli
	echo ${md} > layered_md_device

	# Ensure we can read and write.
	atf_check dd if=testdata of=/dev/${md}.eli.eli bs=$secsize count=1 \
		status=none
	atf_check dd if=/dev/${md}.eli.eli of=cmpdata bs=$secsize count=1 \
		status=none
	atf_check cmp -s testdata cmpdata

	geli detach ${md}.eli 2>/dev/null
}

atf_test_case init cleanup
init_head()
{
	atf_set "descr" "Initialize a geli provider on top of another"
	atf_set "require.user" "root"
	atf_set "timeout" 600
}
init_body()
{
	sectors=2
	geli_test_setup

	for_each_geli_config init_test
}
init_cleanup()
{
	if [ -f layered_md_device ]; then
		while read provider; do
			[ -c /dev/${md}.eli.eli ] && \
				geli detach $md.eli.eli 2>/dev/null
		done < layered_md_device
	fi
	geli_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case init
}
