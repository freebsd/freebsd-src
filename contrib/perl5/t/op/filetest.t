#!./perl

# There are few filetest operators that are portable enough to test.
# See pod/perlport.pod for details.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;
print "1..10\n";

print "not " unless -d 'op';
print "ok 1\n";

print "not " unless -f 'TEST';
print "ok 2\n";

print "not " if -f 'op';
print "ok 3\n";

print "not " if -d 'TEST';
print "ok 4\n";

print "not " unless -r 'TEST';
print "ok 5\n";

# make sure TEST is r-x
eval { chmod 0555, 'TEST' };
$bad_chmod = $@;

$oldeuid = $>;		# root can read and write anything
eval '$> = 1';		# so switch uid (may not be implemented)

print "# oldeuid = $oldeuid, euid = $>\n";

if (!$Config{d_seteuid}) {
    print "ok 6 #skipped, no seteuid\n";
}
elsif ($bad_chmod) {
    print "#[$@]\nok 6 #skipped\n";
}
else {
    print "not " if -w 'TEST';
    print "ok 6\n";
}

# Scripts are not -x everywhere so cannot test that.

eval '$> = $oldeuid';	# switch uid back (may not be implemented)

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
print "not " unless -r 'op';
print "ok 7\n";

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
if ($Config{d_seteuid}) {
    print "not " unless -w 'op';
    print "ok 8\n";
} else {
    print "ok 8 #skipped, no seteuid\n";
}

print "not " unless -x 'op'; # Hohum.  Are directories -x everywhere?
print "ok 9\n";

print "not " unless "@{[grep -r, qw(foo io noo op zoo)]}" eq "io op";
print "ok 10\n";
