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
        if (! $Config{'d_fork'} ||
	    ($Config{'extensions'} !~ /\bIO\b/ && $^O ne 'VMS'))
	{
	    print "1..0\n";
	    exit 0;
        }
    }
}

use IO::Pipe;

my $perl = './perl';

$| = 1;
print "1..10\n";

$pipe = new IO::Pipe->reader($perl, '-e', 'print "not ok 1\n"');
while (<$pipe>) {
  s/^not //;
  print;
}
$pipe->close or print "# \$!=$!\nnot ";
print "ok 2\n";

$cmd = 'BEGIN{$SIG{ALRM} = sub {print "not ok 4\n"; exit}; alarm 10} s/not //';
$pipe = new IO::Pipe->writer($perl, '-pe', $cmd);
print $pipe "not ok 3\n" ;
$pipe->close or print "# \$!=$!\nnot ";
print "ok 4\n";

# Check if can fork with dynamic extensions (bug in CRT):
if ($^O eq 'os2' and
    system "$^X -I../lib -MOpcode -e 'defined fork or die'  > /dev/null 2>&1") {
    print "ok $_ # skipped: broken fork\n" for 5..10;
    exit 0;
}

$pipe = new IO::Pipe;

$pid = fork();

if($pid)
 {
  $pipe->writer;
  print $pipe "Xk 5\n";
  print $pipe "oY 6\n";
  $pipe->close;
  wait;
 }
elsif(defined $pid)
 {
  $pipe->reader;
  $stdin = bless \*STDIN, "IO::Handle";
  $stdin->fdopen($pipe,"r");
  exec 'tr', 'YX', 'ko';
 }
else
 {
  die "# error = $!";
 }

$pipe = new IO::Pipe;
$pid = fork();

if($pid)
 {
  $pipe->reader;
  while(<$pipe>) {
      s/^not //;
      print;
  }
  $pipe->close;
  wait;
 }
elsif(defined $pid)
 {
  $pipe->writer;

  $stdout = bless \*STDOUT, "IO::Handle";
  $stdout->fdopen($pipe,"w");
  print STDOUT "not ok 7\n";
  exec 'echo', 'not ok 8';
 }
else
 {
  die;
 }

$pipe = new IO::Pipe;
$pipe->writer;

$SIG{'PIPE'} = 'broken_pipe';

sub broken_pipe {
    print "ok 9\n";
}

print $pipe "not ok 9\n";
$pipe->close;

sleep 1;

print "ok 10\n";

