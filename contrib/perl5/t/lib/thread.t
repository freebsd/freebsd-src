#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (! $Config{'usethreads'}) {
	print "1..0\n";
	exit 0;
    }

    # XXX known trouble with global destruction
    $ENV{PERL_DESTRUCT_LEVEL} = 0 unless $ENV{PERL_DESTRUCT_LEVEL} > 3;
}
$| = 1;
print "1..14\n";
use Thread;
print "ok 1\n";

sub content
{
 print shift;
 return shift;
}

# create a thread passing args and immedaietly wait for it.
my $t = new Thread \&content,("ok 2\n","ok 3\n", 1..1000);
print $t->join;

# check that lock works ...
{lock $foo;
 $t = new Thread sub { lock $foo; print "ok 5\n" };
 print "ok 4\n";
}
$t->join;

sub dorecurse
{
 my $val = shift;
 my $ret;
 print $val;
 if (@_)
  {
   $ret = Thread->new(\&dorecurse, @_);
   $ret->join;
  }
}

$t = new Thread \&dorecurse, map { "ok $_\n" } 6..10;
$t->join;

# test that sleep lets other thread run
$t = new Thread \&dorecurse,"ok 11\n";
sleep 6;
print "ok 12\n";
$t->join;

sub islocked
{
 use attrs 'locked';
 my $val = shift;
 my $ret;
 print $val;
 if (@_)
  {
   $ret = Thread->new(\&islocked, shift);
  }
 $ret;
}

$t = Thread->new(\&islocked, "ok 13\n", "ok 14\n");
$t->join->join;

