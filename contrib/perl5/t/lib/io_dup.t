#!./perl

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

use IO::Handle;
use IO::File;

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print "1..6\n";

print "ok 1\n";

$dupout = IO::Handle->new->fdopen( \*STDOUT ,"w");
$duperr = IO::Handle->new->fdopen( \*STDERR ,"w");

$stdout = \*STDOUT; bless $stdout, "IO::File"; # "IO::Handle";
$stderr = \*STDERR; bless $stderr, "IO::Handle";

$stdout->open( "Io.dup","w") || die "Can't open stdout";
$stderr->fdopen($stdout,"w");

print $stdout "ok 2\n";
print $stderr "ok 3\n";
if ($^O eq 'MSWin32') {
    print `echo ok 4`;
    print `echo ok 5 1>&2`; # does this *really* work?
}
else {
    system 'echo ok 4';
    system 'echo ok 5 1>&2';
}

$stderr->close;
$stdout->close;

$stdout->fdopen($dupout,"w");
$stderr->fdopen($duperr,"w");

if ($^O eq 'MSWin32') { print `type Io.dup` }
else                  { system 'cat Io.dup' }
unlink 'Io.dup';

print STDOUT "ok 6\n";
