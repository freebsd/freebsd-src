#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
    require Config; import Config;
    if ($] < 5.00326 || not $Config{'d_readdir'}) {
	print "1..0\n";
	exit 0;
    }
}

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

use IO::Dir qw(DIR_UNLINK);

print "1..10\n";

$dot = new IO::Dir ".";
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

open(FH,'>X') || die "Can't create x";
print FH "X";
close(FH);

tie %dir, IO::Dir, ".";
my @files = keys %dir;

# I hope we do not have an empty dir :-)
print @files ? "ok" : "not ok", " 6\n";

my $stat = $dir{'X'};
print defined($stat) && UNIVERSAL::isa($stat,'File::stat') && $stat->size == 1
	? "ok" : "not ok", " 7\n";

delete $dir{'X'};

print -f 'X' ? "ok" : "not ok", " 8\n";

tie %dirx, IO::Dir, ".", DIR_UNLINK;

my $statx = $dirx{'X'};
print defined($statx) && UNIVERSAL::isa($statx,'File::stat') && $statx->size == 1
	? "ok" : "not ok", " 9\n";

delete $dirx{'X'};

print -f 'X' ? "not ok" : "ok", " 10\n";
