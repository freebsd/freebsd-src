tid="GSSAPI Authentication"

# Skip the test if GSSAPI support is not configured
if ! grep -E '^#define GSSAPI' "$BUILDDIR/config.h" >/dev/null 2>&1; then
    skip "GSSAPI not enabled"
fi

# We test with MIT Kerberos KDC, skip if not installed
if ! which krb5kdc >/dev/null 2>&1; then
    skip "MIT Kerberos KDC not installed"
fi

# The test needs nss_wrapper to emulate gethostname() and /etc/hosts,
# we skip if the shared library is not installed
nss_wrapper="libnss_wrapper.so"
if ! ldconfig -p | grep "$nss_wrapper" >/dev/null 2>&1; then
    skip "$nss_wrapper not installed"
fi

# Set up the username of the SSH client
client="$LOGNAME"
if [ "x$client" = "x" ]; then
	client="$(whoami)"
fi

# Set up SSHD and KDC hostnames and resolve both to localhost
sshd_hostname="sshd.example.org"
bad_hostname="bad.example.org"
kdc_hostname="kdc.example.org"
kdc_port=2088
hosts="$OBJ/hosts"
echo "127.0.0.1 $sshd_hostname $kdc_hostname" > "$hosts"

# Set up a directory to store Kerberos data
# (configuration, ticket cache,...)
gssdir="$OBJ/gss"
mkdir -p "$gssdir"
export KRB5CCNAME="$gssdir/cc"
export KRB5_CONFIG="$gssdir/krb5.conf"
export KRB5_KDC_PROFILE="$gssdir/kdc.conf"
export KRB5_KTNAME="$gssdir/ssh.keytab"
export KRB5RCACHETYPE="none"
kdc_pidfile="$gssdir/pid"

# Configure Kerberos
cat<<EOF > "$KRB5_KDC_PROFILE"
[realms]
    EXAMPLE.ORG = {
        database_name = $gssdir/principal
        key_stash_file = $gssdir/stash
        kdc_listen = $kdc_hostname:$kdc_port
        kdc_tcp_listen = $kdc_hostname:$kdc_port
    }
[logging]
    kdc = FILE:$gssdir/kdc.log
    debug = true
EOF

cat<<EOF > "$KRB5_CONFIG"
[libdefaults]
    default_realm = EXAMPLE.ORG
[realms]
    EXAMPLE.ORG = {
        kdc = $kdc_hostname:$kdc_port
    }
EOF

# Back up the default sshd_config
cp "$OBJ/sshd_config" "$OBJ/sshd_config.orig"

setup_sshd() {
    mock_hostname="$1"
    strict_acceptor="$2"

    cp "$OBJ/sshd_config.orig" "$OBJ/sshd_config"

    cat<<EOF >> "$OBJ/sshd_config"
PubkeyAuthentication No
PasswordAuthentication No
GSSAPIAuthentication Yes
EOF

    if ! $strict_acceptor; then
        echo "GSSAPIStrictAcceptorCheck No" >> "$OBJ/sshd_config"
    fi

    test_ssh_sshd_env_backup="$TEST_SSH_SSHD_ENV"
    TEST_SSH_SSHD_ENV="$TEST_SSH_SSHD_ENV                  \
                       LD_PRELOAD=$nss_wrapper             \
                       NSS_WRAPPER_HOSTS=$hosts            \
                       NSS_WRAPPER_HOSTNAME=$mock_hostname \
                       KRB5_CONFIG=$KRB5_CONFIG            \
                       KRB5_KDC_PROFILE=$KRB5_KDC_PROFILE  \
                       KRB5CCNAME=$KRB5CCNAME              \
                       KRB5_KTNAME=$KRB5_KTNAME            \
                       KRB5RCACHETYPE=$KRB5RCACHETYPE"
    start_sshd
}

teardown_sshd() {
    TEST_SSH_SSHD_ENV="$test_ssh_sshd_env_backup"
    stop_sshd
}

setup_kdc() {
    kdb5_util create -P "foo" -s
    krb5kdc -w 1 -P "$kdc_pidfile"
    i=0;
    while [ ! -f "$kdc_pidfile" -a $i -lt 10 ]; do
        i=$((i + 1))
        sleep 1
    done
    test -f "$kdc_pidfile" || fatal "KDC failed to start"
}

teardown_kdc() {
    kill "$(cat "$kdc_pidfile")"
    kdestroy
    rm -f "$KRB5_KTNAME" "$kdc_pidfile"
    kdb5_util destroy -f
}

setup_nss_emulation() {
    export LD_PRELOAD="$nss_wrapper"
    export NSS_WRAPPER_HOSTS="$hosts"
}

teardown_nss_emulation() {
    unset LD_PRELOAD
    unset NSS_WRAPPER_HOSTS
}

setup_krb_principal_with_key() {
    name="$1"
    add_to_keytab="$2"
    kadmin.local add_principal -randkey "$name"
    if $add_to_keytab; then
        kadmin.local ktadd "$name"
    fi
}

setup_krb_principal_with_pw() {
    name="$1"
    password="$2"
    authenticate="$3"
    kadmin.local add_principal -pw "$password" "$name"
    if $authenticate; then
        echo "$password" | kinit "$name"
    fi
}

test_gss_auth() {
    sshd_mock_hostname="$1" # the name that gethostname() will return within sshd
    sshd_principal="$2"     # the hostname for which a Kerberos principal will be created
    auth_sshd="$3"          # whether sshd will be authenticated via a keytab
    auth_client="$4"        # whether the client will be authenticated via kinit
    strict_acceptor="$5"    # whether to be strict about the identity of the sshd server
    expect="$6"             # the expected return value of the sshd command

    setup_sshd "$sshd_mock_hostname" "$strict_acceptor"
    setup_nss_emulation
    setup_kdc

    setup_krb_principal_with_key "host/$sshd_principal" "$auth_sshd"
    setup_krb_principal_with_pw "$client" "foo" "$auth_client"

    ${SSH} -F "$OBJ/ssh_config" -o "GSSAPIAuthentication Yes" "$client@$sshd_hostname" true
    status=$?

    teardown_kdc
    teardown_nss_emulation
    teardown_sshd

    [ $status -eq $expect ]
}

#              sshd_mock_hostname  sshd_principal  auth_sshd  auth_client  strict_acceptor  expect
test_gss_auth  $sshd_hostname      $sshd_hostname  true       true         true             0      \
               || fail "valid authentication attempt failed"
test_gss_auth  $sshd_hostname      $sshd_hostname  false      true         true             255    \
               || fail "authentication succeeded without a keytab entry for the host"
test_gss_auth  $sshd_hostname      $sshd_hostname  true       false        true             255    \
               || fail "authentication succeeded without a ticket-granting ticket"
test_gss_auth  $bad_hostname       $sshd_hostname  true       true         true             255    \
               || fail "authentication succeeded with a hostname/principal mismatch on server side"
test_gss_auth  $bad_hostname       $sshd_hostname  true       true         false            0      \
               || fail "valid authentication without strict acceptor check failed"
test_gss_auth  $bad_hostname       $bad_hostname   true       true         true             255    \
               || fail "authentication succeeded with a hostname/principal mismatch on client side"

unset KRB5CCNAME
unset KRB5_CONFIG
unset KRB5_KDC_PROFILE
unset KRB5_KTNAME
unset KRB5RCACHETYPE
rm -r "$gssdir"
