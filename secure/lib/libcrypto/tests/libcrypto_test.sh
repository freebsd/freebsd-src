# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Enji Cooper

atf_test_case legacy_provider
legacy_provider_head() {
	atf_set "descr" "daemon should drop privileges"
}
legacy_provider_body() {
	local passphrase="test"
	local plaintext="test"

	export OPENSSL_CONF="$PWD/openssl.conf"
	cat > "$OPENSSL_CONF" <<EOF
HOME = .

openssl_conf = openssl_init

[openssl_init]
providers = provider_sect

# List of providers to load
[provider_sect]
default = default_sect
legacy = legacy_sect

[default_sect]
activate = 1

[legacy_sect]
activate = 1
EOF

	echo "$plaintext" | atf_check -s exit:0 -e empty -o not-empty \
	    openssl rc4 -e -k "$passphrase" -a -pbkdf2
}

atf_init_test_cases() {
	atf_add_test_case legacy_provider
}
