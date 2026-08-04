#!/bin/sh

# run in temp dir.
# Then for petal, move into basedir.
# For test_cert.key and test_cert.pem, rename the output files to that.
# And run signit.sh for both signature files, by commenting infile and outfile.
# for test_cert.pem it has emailAddress and keyUsage, but petal.pem does not
# need that.

# settings:

# directory for files
DESTDIR=.

# issuer and subject name for certificates
SERVERNAME=petal
CLIENTNAME=petal

# validity period for certificates
DAYS=7200

# size of keys in bits
BITS=3072

# hash algorithm
HASH=sha256

# base name for unbound server keys
SVR_BASE=petal

# base name for unbound-control keys
CTL_BASE=petal

# flag to recreate generated certificates
RECREATE=0

# we want -rw-r----- access (say you run this as root: grp=yes (server), all=no).
umask 0027

# end of options

set -eu

cleanup() {
    echo "removing artifacts"

    rm -rf \
       server.cnf \
       client.cnf \
       "${SVR_BASE}_trust.pem" \
       "${CTL_BASE}_trust.pem" \
       "${SVR_BASE}_trust.srl"
}

fatal() {
    printf "fatal error: $*\n" >/dev/stderr
    exit 1
}

usage() {
    cat <<EOF
usage: $0 OPTIONS
OPTIONS
-d <dir>  used directory to store keys and certificates (default: $DESTDIR)
-h        show help notice
-r        recreate certificates
EOF
}

OPTIND=1
while getopts 'd:hr' arg; do
    case "$arg" in
      d) DESTDIR="$OPTARG" ;;
      h) usage; exit 1 ;;
      r) RECREATE=1 ;;
      ?) fatal "'$arg' unknown option" ;;
    esac
done
shift $((OPTIND - 1))

if ! openssl version </dev/null >/dev/null 2>&1; then
    echo "$0 requires openssl to be installed for keys/certificates generation." >&2
    exit 1
fi

echo "setup in directory $DESTDIR"
cd "$DESTDIR"

trap cleanup INT

# ===
# Generate server certificate
# ===

# generate private key; do no recreate it if they already exist.
if [ ! -f "$SVR_BASE.key" ]; then
    openssl genrsa -out "$SVR_BASE.key" "$BITS"
fi

cat >server.cnf <<EOF
[req]
default_bits=$BITS
default_md=$HASH
prompt=no
distinguished_name=req_distinguished_name
x509_extensions=v3_ca
[req_distinguished_name]
commonName=$SERVERNAME
emailAddress=$SERVERNAME
[v3_ca]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer:always
basicConstraints=critical,CA:TRUE,pathlen:0
subjectAltName=DNS:$SERVERNAME
keyUsage = digitalSignature, keyCertSign
EOF

[ -f server.cnf ] || fatal "cannot create openssl configuration"

if [ ! -f "$SVR_BASE.pem" -o $RECREATE -eq 1 ]; then
    openssl req \
            -new -x509 \
            -key "$SVR_BASE.key" \
            -config server.cnf  \
            -days "$DAYS" \
            -out "$SVR_BASE.pem"

    [ ! -f "SVR_BASE.pem" ] || fatal "cannot create server certificate"
fi

# ===
# Generate client certificate
# ===

# generate private key; do no recreate it if they already exist.
if [ ! -f "$CTL_BASE.key" ]; then
    openssl genrsa -out "$CTL_BASE.key" "$BITS"
fi

cat >client.cnf <<EOF
[req]
default_bits=$BITS
default_md=$HASH
prompt=no
distinguished_name=req_distinguished_name
req_extensions=v3_req
[req_distinguished_name]
commonName=$CLIENTNAME
[v3_req]
basicConstraints=critical,CA:FALSE
subjectAltName=DNS:$CLIENTNAME
EOF

[ -f client.cnf ] || fatal "cannot create openssl configuration"

if [ ! -f "$CTL_BASE.pem" -o $RECREATE -eq 1 ]; then
    openssl x509 \
        -addtrust serverAuth \
        -in "$SVR_BASE.pem" \
        -out "${SVR_BASE}_trust.pem"

    openssl req \
            -new \
            -config client.cnf \
            -key "$CTL_BASE.key" \
        | openssl x509 \
                  -req \
                  -days "$DAYS" \
                  -CA "${SVR_BASE}_trust.pem" \
                  -CAkey "$SVR_BASE.key" \
                  -CAcreateserial \
                  -$HASH \
                  -extfile client.cnf \
                  -extensions v3_req \
                  -out "$CTL_BASE.pem"

    [ ! -f "CTL_BASE.pem" ] || fatal "cannot create signed client certificate"
fi

# remove unused permissions
chmod o-rw \
      "$SVR_BASE.pem" \
      "$SVR_BASE.key"
chmod g+r,o-rw \
      "$CTL_BASE.pem" \
      "$CTL_BASE.key"

cleanup

echo "Setup success. Certificates created. Enable in unbound.conf file to use"

# create trusted usage pem
# openssl x509 -in $CTL_BASE.pem -addtrust clientAuth -out $CTL_BASE"_trust.pem"

# see details with openssl x509 -noout -text < $SVR_BASE.pem
# echo "create $CTL_BASE""_browser.pfx (web client certificate)"
# echo "create webbrowser PKCS#12 .PFX certificate file. In Firefox import in:"
# echo "preferences - advanced - encryption - view certificates - your certs"
# echo "empty password is used, simply click OK on the password dialog box."
# openssl pkcs12 -export -in $CTL_BASE"_trust.pem" -inkey $CTL_BASE.key -name "unbound remote control client cert" -out $CTL_BASE"_browser.pfx" -password "pass:" || error "could not create browser certificate"

