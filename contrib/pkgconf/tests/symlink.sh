#!/usr/bin/env atf-sh

. $(atf_get_srcdir)/test_env.sh

tests_init \
	pcfiledir_symlink_absolute \
	pcfiledir_symlink_relative

# - We need to create a temporary subtree, since symlinks are not preserved
#   in "make dist".
# - ${srcdir} is relative and since we need to compare paths, we would have
#   to portably canonicalize it again, which is hard. Instead, just keep
#   the whole thing nested.
pcfiledir_symlink_absolute_body()
{
	mkdir -p tmp/child
	cp -f "${selfdir}/lib1/pcfiledir.pc" tmp/child/
	ln -f -s "${PWD}/tmp/child/pcfiledir.pc" tmp/pcfiledir.pc  # absolute
	ln -f -s tmp/pcfiledir.pc pcfiledir.pc

	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir.pc
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix tmp/pcfiledir.pc
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix tmp/child/pcfiledir.pc

	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix "${PWD}/pcfiledir.pc"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix "${PWD}/tmp/pcfiledir.pc"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix "${PWD}/tmp/child/pcfiledir.pc"

	export PKG_CONFIG_PATH="."
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
	export PKG_CONFIG_PATH="${PWD}"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir

	export PKG_CONFIG_PATH="tmp"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
	export PKG_CONFIG_PATH="${PWD}/tmp"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir

	export PKG_CONFIG_PATH="tmp/child"
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
	export PKG_CONFIG_PATH="${PWD}/tmp/child"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
}

pcfiledir_symlink_relative_body()
{
	mkdir -p tmp/child
	cp -f "${selfdir}/lib1/pcfiledir.pc" tmp/child/
	ln -f -s child/pcfiledir.pc tmp/pcfiledir.pc  # relative
	ln -f -s tmp/pcfiledir.pc pcfiledir.pc

	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix pcfiledir.pc
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix tmp/pcfiledir.pc
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix tmp/child/pcfiledir.pc

	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix "${PWD}/pcfiledir.pc"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix "${PWD}/tmp/pcfiledir.pc"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix "${PWD}/tmp/child/pcfiledir.pc"

	export PKG_CONFIG_PATH="."
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
	export PKG_CONFIG_PATH="${PWD}"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir

	export PKG_CONFIG_PATH="tmp"
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
	export PKG_CONFIG_PATH="${PWD}/tmp"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir

	export PKG_CONFIG_PATH="tmp/child"
	atf_check \
		-o inline:"tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
	export PKG_CONFIG_PATH="${PWD}/tmp/child"
	atf_check \
		-o inline:"${PWD}/tmp/child\n" \
		pkgconf --variable=prefix pcfiledir
}
