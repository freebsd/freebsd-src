#!./perl
#
# Tests for Perl run-time environment variable settings
#
# $PERL5OPT, $PERL5LIB, etc.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'d_fork'}) {
        print "1..0 # Skip: no fork\n";
	    exit 0;
    }
}

my $STDOUT = './results-0';
my $STDERR = './results-1';
my $PERL = './perl';
my $FAILURE_CODE = 119;

print "1..9\n";

# Run perl with specified environment and arguments returns a list.
# First element is true iff Perl's stdout and stderr match the
# supplied $stdout and $stderr argument strings exactly.
# second element is an explanation of the failure
sub runperl {
  local *F;
  my ($env, $args, $stdout, $stderr) = @_;

  unshift @$args, '-I../lib';

  $stdout = '' unless defined $stdout;
  $stderr = '' unless defined $stderr;
  my $pid = fork;
  return (0, "Couldn't fork: $!") unless defined $pid;   # failure
  if ($pid) {                   # parent
    my ($actual_stdout, $actual_stderr);
    wait;
    return (0, "Failure in child.\n") if ($?>>8) == $FAILURE_CODE;

    open F, "< $STDOUT" or return (0, "Couldn't read $STDOUT file");
    { local $/; $actual_stdout = <F> }
    open F, "< $STDERR" or return (0, "Couldn't read $STDERR file");
    { local $/; $actual_stderr = <F> }

    if ($actual_stdout ne $stdout) {
      return (0, "Stdout mismatch: expected [$stdout], saw [$actual_stdout]");
    } elsif ($actual_stderr ne $stderr) {
      return (0, "Stderr mismatch: expected [$stderr], saw [$actual_stderr]");
    } else {
      return 1;                 # success
    }
  } else {                      # child
    for my $k (keys %$env) {
      $ENV{$k} = $env->{$k};
    }
    open STDOUT, "> $STDOUT" or exit $FAILURE_CODE;
    open STDERR, "> $STDERR" or it_didnt_work();
    { exec $PERL, @$args }
    it_didnt_work();
  }
}


sub it_didnt_work {
    print STDOUT "IWHCWJIHCI\cNHJWCJQWKJQJWCQW\n";
    exit $FAILURE_CODE;
}

sub try {
  my $testno = shift;
  my ($success, $reason) = runperl(@_);
  if ($success) {
    print "ok $testno\n";
  } else {
    $reason =~ s/\n/\\n/g;
    print "not ok $testno # $reason\n";    
  }
}

#  PERL5OPT    Command-line options (switches).  Switches in
#                    this variable are taken as if they were on
#                    every Perl command line.  Only the -[DIMUdmw]
#                    switches are allowed.  When running taint
#                    checks (because the program was running setuid
#                    or setgid, or the -T switch was used), this
#                    variable is ignored.  If PERL5OPT begins with
#                    -T, tainting will be enabled, and any
#                    subsequent options ignored.

my  $T = 1;
try($T++, {PERL5OPT => '-w'}, ['-e', 'print $::x'],
    "", 
    qq{Name "main::x" used only once: possible typo at -e line 1.\nUse of uninitialized value in print at -e line 1.\n});

try($T++, {PERL5OPT => '-Mstrict'}, ['-e', 'print $::x'],
    "", "");

try($T++, {PERL5OPT => '-Mstrict'}, ['-e', 'print $x'],
    "", 
    qq{Global symbol "\$x" requires explicit package name at -e line 1.\nExecution of -e aborted due to compilation errors.\n});

# Fails in 5.6.0
try($T++, {PERL5OPT => '-Mstrict -w'}, ['-e', 'print $x'],
    "", 
    qq{Global symbol "\$x" requires explicit package name at -e line 1.\nExecution of -e aborted due to compilation errors.\n});

# Fails in 5.6.0
try($T++, {PERL5OPT => '-w -Mstrict'}, ['-e', 'print $::x'],
    "", 
    <<ERROR
Name "main::x" used only once: possible typo at -e line 1.
Use of uninitialized value in print at -e line 1.
ERROR
    );

# Fails in 5.6.0
try($T++, {PERL5OPT => '-w -Mstrict'}, ['-e', 'print $::x'],
    "", 
    <<ERROR
Name "main::x" used only once: possible typo at -e line 1.
Use of uninitialized value in print at -e line 1.
ERROR
    );

try($T++, {PERL5OPT => '-MExporter'}, ['-e0'],
    "", 
    "");

# Fails in 5.6.0
try($T++, {PERL5OPT => '-MExporter -MExporter'}, ['-e0'],
    "", 
    "");

try($T++, {PERL5OPT => '-Mstrict -Mwarnings'}, 
    ['-e', 'print "ok" if $INC{"strict.pm"} and $INC{"warnings.pm"}'],
    "ok",
    "");

print "# ", $T-1, " tests total.\n";

END {
    1 while unlink $STDOUT;
    1 while unlink $STDERR;
}
