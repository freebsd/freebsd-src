#!./perl
#                              -*- Mode: Perl -*-
# closure.t:
#   Original written by Ulrich Pfeifer on 2 Jan 1997.
#   Greatly extended by Tom Phoenix <rootbeer@teleport.com> on 28 Jan 1997.
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;

print "1..171\n";

my $test = 1;
sub test (&) {
  print ((&{$_[0]})?"ok $test\n":"not ok $test\n");
  $test++;
}

my $i = 1;
sub foo { $i = shift if @_; $i }

# no closure
test { foo == 1 };
foo(2);
test { foo == 2 };

# closure: lexical outside sub
my $foo = sub {$i = shift if @_; $i };
my $bar = sub {$i = shift if @_; $i };
test {&$foo() == 2 };
&$foo(3);
test {&$foo() == 3 };
# did the lexical change?
test { foo == 3 and $i == 3};
# did the second closure notice?
test {&$bar() == 3 };

# closure: lexical inside sub
sub bar {
  my $i = shift;
  sub { $i = shift if @_; $i }
}

$foo = bar(4);
$bar = bar(5);
test {&$foo() == 4 };
&$foo(6);
test {&$foo() == 6 };
test {&$bar() == 5 };

# nested closures
sub bizz {
  my $i = 7;
  if (@_) {
    my $i = shift;
    sub {$i = shift if @_; $i };
  } else {
    my $i = $i;
    sub {$i = shift if @_; $i };
  }
}
$foo = bizz();
$bar = bizz();
test {&$foo() == 7 };
&$foo(8);
test {&$foo() == 8 };
test {&$bar() == 7 };

$foo = bizz(9);
$bar = bizz(10);
test {&$foo(11)-1 == &$bar()};

my @foo;
for (qw(0 1 2 3 4)) {
  my $i = $_;
  $foo[$_] = sub {$i = shift if @_; $i };
}

test {
  &{$foo[0]}() == 0 and
  &{$foo[1]}() == 1 and
  &{$foo[2]}() == 2 and
  &{$foo[3]}() == 3 and
  &{$foo[4]}() == 4
  };

for (0 .. 4) {
  &{$foo[$_]}(4-$_);
}

test {
  &{$foo[0]}() == 4 and
  &{$foo[1]}() == 3 and
  &{$foo[2]}() == 2 and
  &{$foo[3]}() == 1 and
  &{$foo[4]}() == 0
  };

sub barf {
  my @foo;
  for (qw(0 1 2 3 4)) {
    my $i = $_;
    $foo[$_] = sub {$i = shift if @_; $i };
  }
  @foo;
}

@foo = barf();
test {
  &{$foo[0]}() == 0 and
  &{$foo[1]}() == 1 and
  &{$foo[2]}() == 2 and
  &{$foo[3]}() == 3 and
  &{$foo[4]}() == 4
  };

for (0 .. 4) {
  &{$foo[$_]}(4-$_);
}

test {
  &{$foo[0]}() == 4 and
  &{$foo[1]}() == 3 and
  &{$foo[2]}() == 2 and
  &{$foo[3]}() == 1 and
  &{$foo[4]}() == 0
  };

# test if closures get created in optimized for loops

my %foo;
for my $n ('A'..'E') {
    $foo{$n} = sub { $n eq $_[0] };
}

test {
  &{$foo{A}}('A') and
  &{$foo{B}}('B') and
  &{$foo{C}}('C') and
  &{$foo{D}}('D') and
  &{$foo{E}}('E')
};

for my $n (0..4) {
    $foo[$n] = sub { $n == $_[0] };
}

test {
  &{$foo[0]}(0) and
  &{$foo[1]}(1) and
  &{$foo[2]}(2) and
  &{$foo[3]}(3) and
  &{$foo[4]}(4)
};

for my $n (0..4) {
    $foo[$n] = sub {
                     # no intervening reference to $n here
                     sub { $n == $_[0] }
		   };
}

test {
  $foo[0]->()->(0) and
  $foo[1]->()->(1) and
  $foo[2]->()->(2) and
  $foo[3]->()->(3) and
  $foo[4]->()->(4)
};

{
    my $w;
    $w = sub {
	my ($i) = @_;
	test { $i == 10 };
	sub { $w };
    };
    $w->(10);
}

# Additional tests by Tom Phoenix <rootbeer@teleport.com>.

{
    use strict;

    use vars qw!$test!;
    my($debugging, %expected, $inner_type, $where_declared, $within);
    my($nc_attempt, $call_outer, $call_inner, $undef_outer);
    my($code, $inner_sub_test, $expected, $line, $errors, $output);
    my(@inners, $sub_test, $pid);
    $debugging = 1 if defined($ARGV[0]) and $ARGV[0] eq '-debug';

    # The expected values for these tests
    %expected = (
	'global_scalar'	=> 1001,
	'global_array'	=> 2101,
	'global_hash'	=> 3004,
	'fs_scalar'	=> 4001,
	'fs_array'	=> 5101,
	'fs_hash'	=> 6004,
	'sub_scalar'	=> 7001,
	'sub_array'	=> 8101,
	'sub_hash'	=> 9004,
	'foreach'	=> 10011,
    );

    # Our innermost sub is either named or anonymous
    for $inner_type (qw!named anon!) {
      # And it may be declared at filescope, within a named
      # sub, or within an anon sub
      for $where_declared (qw!filescope in_named in_anon!) {
	# And that, in turn, may be within a foreach loop,
	# a naked block, or another named sub
	for $within (qw!foreach naked other_sub!) {

	  # Here are a number of variables which show what's
	  # going on, in a way.
	  $nc_attempt = 0+		# Named closure attempted
	      ( ($inner_type eq 'named') ||
	      ($within eq 'other_sub') ) ;
	  $call_inner = 0+		# Need to call &inner
	      ( ($inner_type eq 'anon') &&
	      ($within eq 'other_sub') ) ;
	  $call_outer = 0+		# Need to call &outer or &$outer
	      ( ($inner_type eq 'anon') &&
	      ($within ne 'other_sub') ) ;
	  $undef_outer = 0+		# $outer is created but unused
	      ( ($where_declared eq 'in_anon') &&
	      (not $call_outer) ) ;

	  $code = "# This is a test script built by t/op/closure.t\n\n";

	  $code .= <<"DEBUG_INFO" if $debugging;
# inner_type: $inner_type 
# where_declared: $where_declared 
# within: $within
# nc_attempt: $nc_attempt
# call_inner: $call_inner
# call_outer: $call_outer
# undef_outer: $undef_outer
DEBUG_INFO

	  $code .= <<"END_MARK_ONE";

BEGIN { \$SIG{__WARN__} = sub { 
    my \$msg = \$_[0];
END_MARK_ONE

	  $code .=  <<"END_MARK_TWO" if $nc_attempt;
    return if index(\$msg, 'will not stay shared') != -1;
    return if index(\$msg, 'may be unavailable') != -1;
END_MARK_TWO

	  $code .= <<"END_MARK_THREE";		# Backwhack a lot!
    print "not ok: got unexpected warning \$msg\\n";
} }

{
    my \$test = $test;
    sub test (&) {
      my \$result = &{\$_[0]};
      print "not " unless \$result;
      print "ok \$test\\n";
      \$test++;
    }
}

# some of the variables which the closure will access
\$global_scalar = 1000;
\@global_array = (2000, 2100, 2200, 2300);
%global_hash = 3000..3009;

my \$fs_scalar = 4000;
my \@fs_array = (5000, 5100, 5200, 5300);
my %fs_hash = 6000..6009;

END_MARK_THREE

	  if ($where_declared eq 'filescope') {
	    # Nothing here
	  } elsif ($where_declared eq 'in_named') {
	    $code .= <<'END';
sub outer {
  my $sub_scalar = 7000;
  my @sub_array = (8000, 8100, 8200, 8300);
  my %sub_hash = 9000..9009;
END
    # }
	  } elsif ($where_declared eq 'in_anon') {
	    $code .= <<'END';
$outer = sub {
  my $sub_scalar = 7000;
  my @sub_array = (8000, 8100, 8200, 8300);
  my %sub_hash = 9000..9009;
END
    # }
	  } else {
	    die "What was $where_declared?"
	  }

	  if ($within eq 'foreach') {
	    $code .= "
      my \$foreach = 12000;
      my \@list = (10000, 10010);
      foreach \$foreach (\@list) {
    " # }
	  } elsif ($within eq 'naked') {
	    $code .= "  { # naked block\n"	# }
	  } elsif ($within eq 'other_sub') {
	    $code .= "  sub inner_sub {\n"	# }
	  } else {
	    die "What was $within?"
	  }

	  $sub_test = $test;
	  @inners = ( qw!global_scalar global_array global_hash! ,
	    qw!fs_scalar fs_array fs_hash! );
	  push @inners, 'foreach' if $within eq 'foreach';
	  if ($where_declared ne 'filescope') {
	    push @inners, qw!sub_scalar sub_array sub_hash!;
	  }
	  for $inner_sub_test (@inners) {

	    if ($inner_type eq 'named') {
	      $code .= "    sub named_$sub_test "
	    } elsif ($inner_type eq 'anon') {
	      $code .= "    \$anon_$sub_test = sub "
	    } else {
	      die "What was $inner_type?"
	    }

	    # Now to write the body of the test sub
	    if ($inner_sub_test eq 'global_scalar') {
	      $code .= '{ ++$global_scalar }'
	    } elsif ($inner_sub_test eq 'fs_scalar') {
	      $code .= '{ ++$fs_scalar }'
	    } elsif ($inner_sub_test eq 'sub_scalar') {
	      $code .= '{ ++$sub_scalar }'
	    } elsif ($inner_sub_test eq 'global_array') {
	      $code .= '{ ++$global_array[1] }'
	    } elsif ($inner_sub_test eq 'fs_array') {
	      $code .= '{ ++$fs_array[1] }'
	    } elsif ($inner_sub_test eq 'sub_array') {
	      $code .= '{ ++$sub_array[1] }'
	    } elsif ($inner_sub_test eq 'global_hash') {
	      $code .= '{ ++$global_hash{3002} }'
	    } elsif ($inner_sub_test eq 'fs_hash') {
	      $code .= '{ ++$fs_hash{6002} }'
	    } elsif ($inner_sub_test eq 'sub_hash') {
	      $code .= '{ ++$sub_hash{9002} }'
	    } elsif ($inner_sub_test eq 'foreach') {
	      $code .= '{ ++$foreach }'
	    } else {
	      die "What was $inner_sub_test?"
	    }
	  
	    # Close up
	    if ($inner_type eq 'anon') {
	      $code .= ';'
	    }
	    $code .= "\n";
	    $sub_test++;	# sub name sequence number

	  } # End of foreach $inner_sub_test

	  # Close up $within block		# {
	  $code .= "  }\n\n";

	  # Close up $where_declared block
	  if ($where_declared eq 'in_named') {	# {
	    $code .= "}\n\n";
	  } elsif ($where_declared eq 'in_anon') {	# {
	    $code .= "};\n\n";
	  }

	  # We may need to do something with the sub we just made...
	  $code .= "undef \$outer;\n" if $undef_outer;
	  $code .= "&inner_sub;\n" if $call_inner;
	  if ($call_outer) {
	    if ($where_declared eq 'in_named') {
	      $code .= "&outer;\n\n";
	    } elsif ($where_declared eq 'in_anon') {
	      $code .= "&\$outer;\n\n"
	    }
	  }

	  # Now, we can actually prep to run the tests.
	  for $inner_sub_test (@inners) {
	    $expected = $expected{$inner_sub_test} or
	      die "expected $inner_sub_test missing";

	    # Named closures won't access the expected vars
	    if ( $nc_attempt and 
		substr($inner_sub_test, 0, 4) eq "sub_" ) {
	      $expected = 1;
	    }

	    # If you make a sub within a foreach loop,
	    # what happens if it tries to access the 
	    # foreach index variable? If it's a named
	    # sub, it gets the var from "outside" the loop,
	    # but if it's anon, it gets the value to which
	    # the index variable is aliased.
	    #
	    # Of course, if the value was set only
	    # within another sub which was never called,
	    # the value has not been set yet.
	    #
	    if ($inner_sub_test eq 'foreach') {
	      if ($inner_type eq 'named') {
		if ($call_outer || ($where_declared eq 'filescope')) {
		  $expected = 12001
		} else {
		  $expected = 1
		}
	      }
	    }

	    # Here's the test:
	    if ($inner_type eq 'anon') {
	      $code .= "test { &\$anon_$test == $expected };\n"
	    } else {
	      $code .= "test { &named_$test == $expected };\n"
	    }
	    $test++;
	  }

	  if ($Config{d_fork} and $^O ne 'VMS' and $^O ne 'MSWin32') {
	    # Fork off a new perl to run the tests.
	    # (This is so we can catch spurious warnings.)
	    $| = 1; print ""; $| = 0; # flush output before forking
	    pipe READ, WRITE or die "Can't make pipe: $!";
	    pipe READ2, WRITE2 or die "Can't make second pipe: $!";
	    die "Can't fork: $!" unless defined($pid = open PERL, "|-");
	    unless ($pid) {
	      # Child process here. We're going to send errors back
	      # through the extra pipe.
	      close READ;
	      close READ2;
	      open STDOUT, ">&WRITE"  or die "Can't redirect STDOUT: $!";
	      open STDERR, ">&WRITE2" or die "Can't redirect STDERR: $!";
	      exec './perl', '-w', '-'
		or die "Can't exec ./perl: $!";
	    } else {
	      # Parent process here.
	      close WRITE;
	      close WRITE2;
	      print PERL $code;
	      close PERL;
	      { local $/;
	        $output = join '', <READ>;
	        $errors = join '', <READ2>; }
	      close READ;
	      close READ2;
	    }
	  } else {
	    # No fork().  Do it the hard way.
	    my $cmdfile = "tcmd$$";  $cmdfile++ while -e $cmdfile;
	    my $errfile = "terr$$";  $errfile++ while -e $errfile;
	    my @tmpfiles = ($cmdfile, $errfile);
	    open CMD, ">$cmdfile"; print CMD $code; close CMD;
	    my $cmd = (($^O eq 'VMS') ? "MCR $^X"
		       : ($^O eq 'MSWin32') ? '.\perl'
		       : './perl');
	    $cmd .= " -w $cmdfile 2>$errfile";
	    if ($^O eq 'VMS' or $^O eq 'MSWin32') {
	      # Use pipe instead of system so we don't inherit STD* from
	      # this process, and then foul our pipe back to parent by
	      # redirecting output in the child.
	      open PERL,"$cmd |" or die "Can't open pipe: $!\n";
	      { local $/; $output = join '', <PERL> }
	      close PERL;
	    } else {
	      my $outfile = "tout$$";  $outfile++ while -e $outfile;
	      push @tmpfiles, $outfile;
	      system "$cmd >$outfile";
	      { local $/; open IN, $outfile; $output = <IN>; close IN }
	    }
	    if ($?) {
	      printf "not ok: exited with error code %04X\n", $?;
	      $debugging or do { 1 while unlink @tmpfiles };
	      exit;
	    }
	    { local $/; open IN, $errfile; $errors = <IN>; close IN }
	    1 while unlink @tmpfiles;
	  }
	  print $output;
	  print STDERR $errors;
	  if ($debugging && ($errors || $? || ($output =~ /not ok/))) {
	    my $lnum = 0;
	    for $line (split '\n', $code) {
	      printf "%3d:  %s\n", ++$lnum, $line;
	    }
	  }
	  printf "not ok: exited with error code %04X\n", $? if $?;
	  print "-" x 30, "\n" if $debugging;

	}	# End of foreach $within
      }	# End of foreach $where_declared
    }	# End of foreach $inner_type

}

