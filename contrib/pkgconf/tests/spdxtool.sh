#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	basic \
	with_dependency \
	meta_package

basic_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib-sbom"
	atf_check \
		-o file:"${selfdir}/lib-sbom-files/basic.json" \
		spdxtool --creation-time="test" test3
}

with_dependency_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib-sbom"
	atf_check \
		-o file:"${selfdir}/lib-sbom-files/with_dependency.json" \
		spdxtool --creation-time="test" test2
}

meta_package_body()
{
	export PKG_CONFIG_PATH="${selfdir}/lib-sbom"
	atf_check \
		-o file:"${selfdir}/lib-sbom-files/meta_package.json" \
		spdxtool --creation-time="test" meta_package
}
