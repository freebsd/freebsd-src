#!/usr/bin/suidperl
#
# Search /etc/skeykeys for the skey string for this user OR user specified
# in 1st parameter.
#
# $Id: keyinfo.pl,v 1.1 1996/04/26 21:35:10 joerg Exp $
#

die "usage: keyinfo [user]\n" unless $#ARGV < 1;

open(K, "/etc/skeykeys") || exit 1;

if ($#ARGV == 0) {
    $user = $ARGV[0];
} else {
    $user = (getpwuid($<))[0];
}

while (<K>) {
    ($id, $seq, $serial) = split;
    if ($id eq $user) {
	printf "%d %s\n", $seq - 1, $serial;
	exit 0;
    }
}
exit 1;

