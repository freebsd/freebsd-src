#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib' if -d '../lib';
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

use IO::File;
use IO::Seekable;

print "1..4\n";

$x = new_tmpfile IO::File or print "not ";
print "ok 1\n";
print $x "ok 2\n";
$x->seek(0,SEEK_SET);
print <$x>;

$x->seek(0,SEEK_SET);
print $x "not ok 3\n";
$p = $x->getpos;
print $x "ok 3\n";
$x->flush;
$x->setpos($p);
print scalar <$x>;

$! = 0;
$x->setpos(undef);
print $! ? "ok 4 # $!\n" : "not ok 4\n";
