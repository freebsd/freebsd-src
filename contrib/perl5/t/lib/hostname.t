#!./perl

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
}

use Sys::Hostname;

eval {
    $host = hostname;
};

if ($@) {
    print "1..0\n" if $@ =~ /Cannot get host name/;
} else {
    print "1..1\n";
    print "# \$host = `$host'\n";
    print "ok 1\n";
}
