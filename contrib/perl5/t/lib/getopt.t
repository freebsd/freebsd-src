#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..11\n";

use Getopt::Std;

# First we test the getopt function
@ARGV = qw(-xo -f foo -y file);
getopt('f');

print "not " if "@ARGV" ne 'file';
print "ok 1\n";

print "not " unless $opt_x && $opt_o && opt_y;
print "ok 2\n";

print "not " unless $opt_f eq 'foo';
print "ok 3\n";


# Then we try the getopts
$opt_o = $opt_i = $opt_f = undef;
@ARGV = qw(-foi -i file);
getopts('oif:') or print "not ";
print "ok 4\n";

print "not " unless "@ARGV" eq 'file';
print "ok 5\n";

print "not " unless $opt_i and $opt_f eq 'oi';
print "ok 6\n";

print "not " if $opt_o;
print "ok 7\n";

# Try illegal options, but avoid printing of the error message

open(STDERR, ">stderr") || die;

@ARGV = qw(-h help);

!getopts("xf:y") or print "not ";
print "ok 8\n";


# Then try the Getopt::Long module

use Getopt::Long;

@ARGV = qw(--help --file foo --foo --nobar --num=5 -- file);

GetOptions(
   'help'   => \$HELP,
   'file:s' => \$FILE,
   'foo!'   => \$FOO,
   'bar!'   => \$BAR,
   'num:i'  => \$NO,
) || print "not ";
print "ok 9\n";

print "not " unless $HELP && $FOO && !$BAR && $FILE eq 'foo' && $NO == 5;
print "ok 10\n";

print "not " unless "@ARGV" eq "file";
print "ok 11\n";

close STDERR;
unlink "stderr";
