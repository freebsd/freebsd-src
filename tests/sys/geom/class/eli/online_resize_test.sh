#!/bin/sh

. $(atf_get_srcdir)/conf.sh

atf_test_case online_resize cleanup
online_resize_head()
{
	atf_set "descr" "online resize of geli providers"
	atf_set "require.user" "root"
}
online_resize_body()
{
	geli_test_setup

	(
		echo "m 512 none 10485248 1 1 20971008 1 1 31456768 1 1"
		echo "m 4096 none 10481664 1 1 20967424 1 1 31453184 1 1"
		echo "m 512 HMAC/SHA256 5242368 1 1 10485248 1 1 15728128 1 1"
		echo "m 4096 HMAC/SHA256 9318400 1 1 18640896 1 1 27959296 1 1"
		echo "p 512 none 11258999068425728 [0-9] 20971520 22517998136851968 [0-9] 41943040 33776997205278208 [0-9] 62914560"
		echo "p 4096 none 11258999068422144 [0-9] 2621440 22517998136848384 [0-9] 5242880 33776997205274624 [0-9] 7864320"
		echo "p 512 HMAC/SHA256 5629499534212608 [0-9] 20971520 11258999068425728 [0-9] 41943040 16888498602638848 [0-9] 62914560"
		echo "p 4096 HMAC/SHA256 10007999171932160 [0-9] 20971520 20015998343868416 [0-9] 41943040 30023997515800576 [0-9] 62914560"
	) | while read prefix sector auth esize10 ka10 kt10 esize20 ka20 kt20 esize30 ka30 kt30; do
		if [ "${auth}" = "none" ]; then
			aalgo=""
			eflags="0x200"
			dflags="0x0"
		else
			aalgo="-a ${auth}"
			eflags="0x210"
			dflags="0x10"
		fi

		if [ "${prefix}" = "m" ]; then
			psize10="10485760"
			psize20="20971520"
			psize30="31457280"
		else
			psize10="11258999068426240"
			psize20="22517998136852480"
			psize30="33776997205278720"
		fi

		md=$(attach_md -t malloc -s40${prefix})

		# Initialise
		atf_check -s exit:0 -o ignore gpart create -s GPT ${md}
		atf_check -s exit:0 -o ignore gpart add -t freebsd-ufs -s 10${prefix} ${md}

		echo secret >tmp.key

		atf_check geli init ${aalgo} -s ${sector} -Bnone -PKtmp.key ${md}p1
		# Autoresize is set by default.
		atf_check -s exit:0 -o match:"flags: ${eflags}$" geli dump ${md}p1

		atf_check geli configure -R ${md}p1
		atf_check -s exit:0 -o match:"flags: ${dflags}$" geli dump ${md}p1
		atf_check geli configure -r ${md}p1
		atf_check -s exit:0 -o match:"flags: ${eflags}$" geli dump ${md}p1

		atf_check geli init -R ${aalgo} -s ${sector} -Bnone -PKtmp.key ${md}p1
		atf_check -s exit:0 -o match:"flags: ${dflags}$" geli dump ${md}p1

		atf_check geli configure -r ${md}p1
		atf_check -s exit:0 -o match:"flags: ${eflags}$" geli dump ${md}p1
		atf_check geli configure -R ${md}p1
		atf_check -s exit:0 -o match:"flags: ${dflags}$" geli dump ${md}p1

		atf_check geli init ${aalgo} -s ${sector} -Bnone -PKtmp.key ${md}p1
		atf_check geli attach -pk tmp.key ${md}p1
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check geli configure -R ${md}p1
		atf_check -s exit:0 -o match:"flags: ${dflags}$" geli dump ${md}p1
		atf_check -o not-match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check geli configure -r ${md}p1
		atf_check -s exit:0 -o match:"flags: ${eflags}$" geli dump ${md}p1
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli

		atf_check geli configure -R ${md}p1
		atf_check -s exit:0 -o match:"provsize: ${psize10}$" geli dump ${md}p1
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 20${prefix} ${md}
		# Autoresize turned off - we lose metadata.
		atf_check -s exit:1 -o empty -e ignore geli dump ${md}p1
		atf_check geli detach ${md}p1.eli
		# When we recover previous size, the metadata should be there.
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 10${prefix} ${md}
		atf_check -s exit:0 -o match:"flags: ${dflags}$" geli dump ${md}p1

		atf_check geli configure -r ${md}p1
		atf_check geli attach -pk tmp.key ${md}p1
		atf_check -s exit:0 -o match:"^[[:space:]]${esize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:"^KeysAllocated: ${ka10}$" geli list ${md}p1.eli
		atf_check -s exit:0 -o match:"^KeysTotal: ${kt10}$" geli list ${md}p1.eli

		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 20${prefix} ${md}
		atf_check -s exit:0 -o match:"provsize: ${psize20}$" geli dump ${md}p1
		atf_check -s exit:0 -o match:"^[[:space:]]${esize20}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:"^KeysAllocated: ${ka20}$" geli list ${md}p1.eli
		atf_check -s exit:0 -o match:"^KeysTotal: ${kt20}$" geli list ${md}p1.eli
		atf_check -s exit:0 -o match:"flags: ${eflags}$" geli dump ${md}p1
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		if [ "${prefix}" = "m" ]; then
			atf_check -s exit:1 -o empty -e match:"^${esize20} bytes transferred " dd if=/dev/random of=/dev/${md}p1.eli bs=1m
			atf_check -s exit:0 -o empty -e match:"^${esize20} bytes transferred " dd if=/dev/${md}p1.eli of=/dev/null bs=1m
		fi

		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 30${prefix} ${md}
		atf_check -s exit:0 -o match:"provsize: ${psize30}$" geli dump ${md}p1
		atf_check -s exit:0 -o match:"^[[:space:]]${esize30}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:"^KeysAllocated: ${ka30}$" geli list ${md}p1.eli
		atf_check -s exit:0 -o match:"^KeysTotal: ${kt30}$" geli list ${md}p1.eli
		atf_check -s exit:0 -o match:"flags: ${eflags}$" geli dump ${md}p1
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		if [ "${prefix}" = "m" ]; then
			atf_check -s exit:1 -o empty -e match:"^${esize30} bytes transferred " dd if=/dev/random of=/dev/${md}p1.eli bs=1m
			atf_check -s exit:0 -o empty -e match:"^${esize30} bytes transferred " dd if=/dev/${md}p1.eli of=/dev/null bs=1m
		fi

		atf_check geli detach ${md}p1.eli

		# Make sure that the old metadata is removed.
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 20${prefix} ${md}
		atf_check -s exit:1 -o empty -e ignore geli dump ${md}p1
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 10${prefix} ${md}
		atf_check -s exit:1 -o empty -e ignore geli dump ${md}p1

		# Test geli with onetime keys.
		if [ "${auth}" = "none" ]; then
			osize10="${psize10}"
			osize20="${psize20}"
			osize30="${psize30}"
		else
			osize10="${esize10}"
			osize20="${esize20}"
			osize30="${esize30}"
			if [ "${sector}" -eq 512 ]; then
				osize10=$((osize10+sector))
				osize20=$((osize20+sector))
				osize30=$((osize30+sector))
			fi
		fi
		atf_check geli onetime ${aalgo} -s ${sector} ${md}p1
		atf_check -s exit:0 -o match:"^[[:space:]]${osize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 20${prefix} ${md}
		atf_check -s exit:0 -o match:"^[[:space:]]${osize20}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 30${prefix} ${md}
		atf_check -s exit:0 -o match:"^[[:space:]]${osize30}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check geli detach ${md}p1.eli

		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 10${prefix} ${md}
		atf_check geli onetime -R ${aalgo} -s ${sector} ${md}p1
		atf_check -s exit:0 -o match:"^[[:space:]]${osize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -o not-match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 20${prefix} ${md}
		atf_check -s exit:0 -o match:"^[[:space:]]${osize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 30${prefix} ${md}
		atf_check -s exit:0 -o match:"^[[:space:]]${osize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check geli detach ${md}p1.eli

		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 10${prefix} ${md}
		atf_check geli onetime ${aalgo} -s ${sector} ${md}p1
		atf_check -s exit:0 -o match:"^[[:space:]]${osize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check geli configure -R ${md}p1
		atf_check -o not-match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 20${prefix} ${md}
		atf_check -s exit:0 -o match:"^[[:space:]]${osize10}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
		atf_check geli configure -r ${md}p1
		atf_check -s exit:0 -o match:'^Flags: .*AUTORESIZE' geli list ${md}p1.eli
		atf_check -s exit:0 -o match:resized gpart resize -i 1 -s 30${prefix} ${md}
		atf_check -s exit:0 -o match:"^[[:space:]]${osize30}[[:space:]]+# mediasize in bytes" diskinfo -v ${md}p1.eli
	done
}
online_resize_cleanup()
{
	if [ -f "$TEST_MDS_FILE" ]; then
		while read md; do
			atf_check -s ignore -e ignore -o ignore geli detach ${md}p1.eli
			atf_check -s ignore -e ignore -o ignore gpart delete -i 1 ${md}
			atf_check -s ignore -e ignore -o ignore gpart destroy ${md}
		done < $TEST_MDS_FILE
	fi
	geli_test_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case online_resize
}
