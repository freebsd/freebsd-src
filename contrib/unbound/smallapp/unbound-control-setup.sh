#!/bin/sh
#
# unbound-control-setup.sh - set up SSL certificates for unbound-control
#
# Copyright (c) 2008, NLnet Labs. All rights reserved.
#
# This software is open source.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 
# Neither the name of the NLNET LABS nor the names of its contributors may
# be used to endorse or promote products derived from this software without
# specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# settings:

# directory for files
DESTDIR=/var/unbound

# issuer and subject name for certificates
SERVERNAME=unbound
CLIENTNAME=unbound-control

# validity period for certificates
DAYS=7200

# size of keys in bits
BITS=3072

# hash algorithm
HASH=sha256

# base name for unbound server keys
SVR_BASE=unbound_server

# base name for unbound-control keys
CTL_BASE=unbound_control

# we want -rw-r----- access (say you run this as root: grp=yes (server), all=no).
umask 0027

# end of options

# functions:
error ( ) {
	echo "$0 fatal error: $1"
	exit 1
}

# check arguments:
while test $# -ne 0; do
	case $1 in
	-d)
	if test $# -eq 1; then error "need argument for -d"; fi
	DESTDIR="$2"
	shift
	;;
	*)
	echo "unbound-control-setup.sh - setup SSL keys for unbound-control"
	echo "	-d dir	use directory to store keys and certificates."
	echo "		default: $DESTDIR"
	echo "please run this command using the same user id that the "
	echo "unbound daemon uses, it needs read privileges."
	exit 1
	;;
	esac
	shift
done

# go!:
echo "setup in directory $DESTDIR"
cd "$DESTDIR" || error "could not cd to $DESTDIR"

# create certificate keys; do not recreate if they already exist.
if test -f $SVR_BASE.key; then
	echo "$SVR_BASE.key exists"
else
	echo "generating $SVR_BASE.key"
	openssl genrsa -out $SVR_BASE.key $BITS || error "could not genrsa"
fi
if test -f $CTL_BASE.key; then
	echo "$CTL_BASE.key exists"
else
	echo "generating $CTL_BASE.key"
	openssl genrsa -out $CTL_BASE.key $BITS || error "could not genrsa"
fi

# create self-signed cert for server
echo "[req]" > request.cfg
echo "default_bits=$BITS" >> request.cfg
echo "default_md=$HASH" >> request.cfg
echo "prompt=no" >> request.cfg
echo "distinguished_name=req_distinguished_name" >> request.cfg
echo "" >> request.cfg
echo "[req_distinguished_name]" >> request.cfg
echo "commonName=$SERVERNAME" >> request.cfg

test -f request.cfg || error "could not create request.cfg"

echo "create $SVR_BASE.pem (self signed certificate)"
openssl req -key $SVR_BASE.key -config request.cfg  -new -x509 -days $DAYS -out $SVR_BASE.pem || error "could not create $SVR_BASE.pem"
# create trusted usage pem
openssl x509 -in $SVR_BASE.pem -addtrust serverAuth -out $SVR_BASE"_trust.pem"

# create client request and sign it, piped
echo "[req]" > request.cfg
echo "default_bits=$BITS" >> request.cfg
echo "default_md=$HASH" >> request.cfg
echo "prompt=no" >> request.cfg
echo "distinguished_name=req_distinguished_name" >> request.cfg
echo "" >> request.cfg
echo "[req_distinguished_name]" >> request.cfg
echo "commonName=$CLIENTNAME" >> request.cfg

test -f request.cfg || error "could not create request.cfg"

echo "create $CTL_BASE.pem (signed client certificate)"
openssl req -key $CTL_BASE.key -config request.cfg -new | openssl x509 -req -days $DAYS -CA $SVR_BASE"_trust.pem" -CAkey $SVR_BASE.key -CAcreateserial -$HASH -out $CTL_BASE.pem
test -f $CTL_BASE.pem || error "could not create $CTL_BASE.pem"
# create trusted usage pem
# openssl x509 -in $CTL_BASE.pem -addtrust clientAuth -out $CTL_BASE"_trust.pem"

# see details with openssl x509 -noout -text < $SVR_BASE.pem
# echo "create $CTL_BASE""_browser.pfx (web client certificate)"
# echo "create webbrowser PKCS#12 .PFX certificate file. In Firefox import in:"
# echo "preferences - advanced - encryption - view certificates - your certs"
# echo "empty password is used, simply click OK on the password dialog box."
# openssl pkcs12 -export -in $CTL_BASE"_trust.pem" -inkey $CTL_BASE.key -name "unbound remote control client cert" -out $CTL_BASE"_browser.pfx" -password "pass:" || error "could not create browser certificate"

# set desired permissions
chmod 0640 $SVR_BASE.pem $SVR_BASE.key $CTL_BASE.pem $CTL_BASE.key

# remove crap
rm -f request.cfg
rm -f $CTL_BASE"_trust.pem" $SVR_BASE"_trust.pem" $SVR_BASE"_trust.srl"

echo "Setup success. Certificates created. Enable in unbound.conf file to use"

exit 0
