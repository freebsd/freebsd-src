#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (not $Config{'d_readdir'}) {
	print "1..0\n";
	exit 0;
    }
}

use DirHandle;

print "1..5\n";

$dot = new DirHandle ".";
print defined($dot) ? "ok" : "not ok", " 1\n";

@a = sort <*>;
do { $first = $dot->read } while defined($first) && $first =~ /^\./;
print +(grep { $_ eq $first } @a) ? "ok" : "not ok", " 2\n";

@b = sort($first, (grep {/^[^.]/} $dot->read));
print +(join("\0", @a) eq join("\0", @b)) ? "ok" : "not ok", " 3\n";

$dot->rewind;
@c = sort grep {/^[^.]/} $dot->read;
print +(join("\0", @b) eq join("\0", @c)) ? "ok" : "not ok", " 4\n";

$dot->close;
$dot->rewind;
print defined($dot->read) ? "not ok" : "ok", " 5\n";
