#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (! $Config{'use5005threads'}) {
	print "1..0 # Skip: not use5005threads\n";
	exit 0;
    }

    # XXX known trouble with global destruction
    $ENV{PERL_DESTRUCT_LEVEL} = 0 unless $ENV{PERL_DESTRUCT_LEVEL} > 3;
}
$| = 1;
print "1..22\n";
use Thread 'yield';
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

sub islocked : locked {
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

{
    package Loch::Ness;
    sub new { bless [], shift }
    sub monster : locked : method {
	my($s, $m) = @_;
	print "ok $m\n";
    }
    sub gollum { &monster }
}
Loch::Ness->monster(15);
Loch::Ness->new->monster(16);
Loch::Ness->gollum(17);
Loch::Ness->new->gollum(18);

my $short = "This is a long string that goes on and on.";
my $shorte = " a long string that goes on and on.";
my $long  = "This is short.";
my $longe  = " short.";
my $thr1 = new Thread \&threaded, $short, $shorte, "19";
my $thr2 = new Thread \&threaded, $long, $longe, "20";
my $thr3 = new Thread \&testsprintf, "21";

sub testsprintf {
  my $testno = shift;
  # this may coredump if thread vars are not properly initialised
  my $same = sprintf "%.0f", $testno;
  if ($testno eq $same) {
    print "ok $testno\n";
  } else {
    print "not ok $testno\t# '$testno' ne '$same'\n";
  }
}

sub threaded {
  my ($string, $string_end, $testno) = @_;

  # Do the match, saving the output in appropriate variables
  $string =~ /(.*)(is)(.*)/;
  # Yield control, allowing the other thread to fill in the match variables
  yield();
  # Examine the match variable contents; on broken perls this fails
  if ($3 eq $string_end) {
    print "ok $testno\n";
  }
  else {
    warn <<EOT;

#
# This is a KNOWN FAILURE, and one of the reasons why threading
# is still an experimental feature.  It is here to stop people
# from deploying threads in production. ;-)
#
EOT
    print "not ok $testno # other thread filled in match variables\n";
  }
}
$thr1->join;
$thr2->join;
$thr3->join;
print "ok 22\n";
