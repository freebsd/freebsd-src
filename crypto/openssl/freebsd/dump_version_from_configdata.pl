#!/usr/bin/env perl
#
# This dumps out the values needed to generate manpages and other artifacts
# which include the release version/date.
#
# See also: `secure/lib/libcrypto/Makefile.version`.

use Cwd qw(realpath);
use File::Basename qw(dirname);
use Time::Piece;

use lib dirname(dirname(realpath($0)));

use configdata qw(%config);

$OPENSSL_DATE = Time::Piece->strptime($config{"release_date"}, "%d %b %Y")->strftime("%Y-%m-%d");

$OPENSSL_VER = "$config{'major'}.$config{'minor'}.$config{'patch'}";

print("OPENSSL_VER=\t${OPENSSL_VER}\n");
print("OPENSSL_DATE=\t${OPENSSL_DATE}\n");
