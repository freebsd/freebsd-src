#!/bin/perl

#
#  Convert GECOS information in password files to alias syntax.
#
#  Contributed by Kari E. Hurtta <Kari.Hurtta@ozone.fmi.fi>
#

print "# Generated from passwd by $0\n";

while (@a = getpwent) {
    ($name,$passwd,$uid,$gid,$quota,$comment,$gcos,$dir,$shell) = @a;

    ($fullname = $gcos) =~ s/,.*$//;

    if (!-d $dir || !-x $shell) {
	print "$name: root\n";
    }

    $fullname =~ s/\.*[ _]+\.*/./g;
    if ($fullname =~ /^[a-zA-Z]+(\.[a-zA-Z]+)+$/) {
	print "$fullname: $name\n";
    } else {
	print "# $fullname: $name\n";
    }
};

endpwent;
