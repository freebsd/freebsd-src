#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSys\/Hostname\b/) {
      print "1..0 # Skip: Sys::Hostname was not built\n";
      exit 0;
    }
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
