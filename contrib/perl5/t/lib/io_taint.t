#!./perl -T

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use Config;

BEGIN {
    if(-d "lib" && -f "TEST") {
        if ($Config{'extensions'} !~ /\bIO\b/ && $^O ne 'VMS') {
	    print "1..0\n";
	    exit 0;
        }
    }
}

END { unlink "./__taint__$$" }

print "1..3\n";
use IO::File;
$x = new IO::File "> ./__taint__$$" || die("Cannot open ./__taint__$$\n");
print $x "$$\n";
$x->close;

$x = new IO::File "< ./__taint__$$" || die("Cannot open ./__taint__$$\n");
chop($unsafe = <$x>);
eval { kill 0 * $unsafe };
print "not " if $^O ne 'MSWin32' and ($@ !~ /^Insecure/o);
print "ok 1\n";
$x->close;

# We could have just done a seek on $x, but technically we haven't tested
# seek yet...
$x = new IO::File "< ./__taint__$$" || die("Cannot open ./__taint__$$\n");
$x->untaint;
print "not " if ($?);
print "ok 2\n"; # Calling the method worked
chop($unsafe = <$x>);
eval { kill 0 * $unsafe };
print "not " if ($@ =~ /^Insecure/o);
print "ok 3\n"; # No Insecure message from using the data
$x->close;

exit 0;
