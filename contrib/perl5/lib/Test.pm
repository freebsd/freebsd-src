use strict;
package Test;
use Test::Harness 1.1601 ();
use Carp;
use vars (qw($VERSION @ISA @EXPORT $ntest $TestLevel), #public-ish
	  qw($ONFAIL %todo %history $planned @FAILDETAIL)); #private-ish
$VERSION = '1.04';
require Exporter;
@ISA=('Exporter');
@EXPORT= qw(&plan &ok &skip $ntest);

$TestLevel = 0;		# how many extra stack frames to skip
$|=1;
#$^W=1;  ?
$ntest=1;

# Use of this variable is strongly discouraged.  It is set mainly to
# help test coverage analyzers know which test is running.
$ENV{REGRESSION_TEST} = $0;

sub plan {
    croak "Test::plan(%args): odd number of arguments" if @_ & 1;
    croak "Test::plan(): should not be called more than once" if $planned;
    my $max=0;
    for (my $x=0; $x < @_; $x+=2) {
	my ($k,$v) = @_[$x,$x+1];
	if ($k =~ /^test(s)?$/) { $max = $v; }
	elsif ($k eq 'todo' or 
	       $k eq 'failok') { for (@$v) { $todo{$_}=1; }; }
	elsif ($k eq 'onfail') { 
	    ref $v eq 'CODE' or croak "Test::plan(onfail => $v): must be CODE";
	    $ONFAIL = $v; 
	}
	else { carp "Test::plan(): skipping unrecognized directive '$k'" }
    }
    my @todo = sort { $a <=> $b } keys %todo;
    if (@todo) {
	print "1..$max todo ".join(' ', @todo).";\n";
    } else {
	print "1..$max\n";
    }
    ++$planned;
}

sub to_value {
    my ($v) = @_;
    (ref $v or '') eq 'CODE' ? $v->() : $v;
}

# STDERR is NOT used for diagnostic output which should have been
# fixed before release.  Is this appropriate?

sub ok ($;$$) {
    croak "ok: plan before you test!" if !$planned;
    my ($pkg,$file,$line) = caller($TestLevel);
    my $repetition = ++$history{"$file:$line"};
    my $context = ("$file at line $line".
		   ($repetition > 1 ? " fail \#$repetition" : ''));
    my $ok=0;
    my $result = to_value(shift);
    my ($expected,$diag);
    if (@_ == 0) {
	$ok = $result;
    } else {
	$expected = to_value(shift);
	# until regex can be manipulated like objects...
	my ($regex,$ignore);
	if (($regex) = ($expected =~ m,^ / (.+) / $,sx) or
	    ($ignore, $regex) = ($expected =~ m,^ m([^\w\s]) (.+) \1 $,sx)) {
	    $ok = $result =~ /$regex/;
	} else {
	    $ok = $result eq $expected;
	}
    }
    if ($todo{$ntest}) {
	if ($ok) { 
	    print "ok $ntest # Wow! ($context)\n";
	} else {
	    $diag = to_value(shift) if @_;
	    if (!$diag) {
		print "not ok $ntest # (failure expected in $context)\n";
	    } else {
		print "not ok $ntest # (failure expected: $diag)\n";
	    }
	}
    } else {
	print "not " if !$ok;
	print "ok $ntest\n";
	
	if (!$ok) {
	    my $detail = { 'repetition' => $repetition, 'package' => $pkg,
			   'result' => $result };
	    $$detail{expected} = $expected if defined $expected;
	    $diag = $$detail{diagnostic} = to_value(shift) if @_;
	    if (!defined $expected) {
		if (!$diag) {
		    print STDERR "# Failed test $ntest in $context\n";
		} else {
		    print STDERR "# Failed test $ntest in $context: $diag\n";
		}
	    } else {
		my $prefix = "Test $ntest";
		print STDERR "# $prefix got: '$result' ($context)\n";
		$prefix = ' ' x (length($prefix) - 5);
		if (!$diag) {
		    print STDERR "# $prefix Expected: '$expected'\n";
		} else {
		    print STDERR "# $prefix Expected: '$expected' ($diag)\n";
		}
	    }
	    push @FAILDETAIL, $detail;
	}
    }
    ++ $ntest;
    $ok;
}

sub skip ($$;$$) {
    if (to_value(shift)) {
	print "ok $ntest # skip\n";
	++ $ntest;
	1;
    } else {
	local($TestLevel) = $TestLevel+1;  #ignore this stack frame
	&ok;
    }
}

END {
    $ONFAIL->(\@FAILDETAIL) if @FAILDETAIL && $ONFAIL;
}

1;
__END__

=head1 NAME

  Test - provides a simple framework for writing test scripts

=head1 SYNOPSIS

  use strict;
  use Test;
  BEGIN { plan tests => 13, todo => [3,4] }

  ok(0); # failure
  ok(1); # success

  ok(0); # ok, expected failure (see todo list, above)
  ok(1); # surprise success!

  ok(0,1);             # failure: '0' ne '1'
  ok('broke','fixed'); # failure: 'broke' ne 'fixed'
  ok('fixed','fixed'); # success: 'fixed' eq 'fixed'

  ok(sub { 1+1 }, 2);  # success: '2' eq '2'
  ok(sub { 1+1 }, 3);  # failure: '2' ne '3'
  ok(0, int(rand(2));  # (just kidding! :-)

  my @list = (0,0);
  ok @list, 3, "\@list=".join(',',@list);      #extra diagnostics
  ok 'segmentation fault', '/(?i)success/';    #regex match

  skip($feature_is_missing, ...);    #do platform specific test

=head1 DESCRIPTION

Test::Harness expects to see particular output when it executes tests.
This module aims to make writing proper test scripts just a little bit
easier (and less error prone :-).

=head1 TEST TYPES

=over 4

=item * NORMAL TESTS

These tests are expected to succeed.  If they don't, something's
screwed up!

=item * SKIPPED TESTS

Skip tests need a platform specific feature that might or might not be
available.  The first argument should evaluate to true if the required
feature is NOT available.  After the first argument, skip tests work
exactly the same way as do normal tests.

=item * TODO TESTS

TODO tests are designed for maintaining an executable TODO list.
These tests are expected NOT to succeed (otherwise the feature they
test would be on the new feature list, not the TODO list).

Packages should NOT be released with successful TODO tests.  As soon
as a TODO test starts working, it should be promoted to a normal test
and the newly minted feature should be documented in the release
notes.

=back

=head1 ONFAIL

  BEGIN { plan test => 4, onfail => sub { warn "CALL 911!" } }

The test failures can trigger extra diagnostics at the end of the test
run.  C<onfail> is passed an array ref of hash refs that describe each
test failure.  Each hash will contain at least the following fields:
package, repetition, and result.  (The file, line, and test number are
not included because their correspondance to a particular test is
fairly weak.)  If the test had an expected value or a diagnostic
string, these will also be included.

This optional feature might be used simply to print out the version of
your package and/or how to report problems.  It might also be used to
generate extremely sophisticated diagnostics for a particular test
failure.  It's not a panacea, however.  Core dumps or other
unrecoverable errors will prevent the C<onfail> hook from running.
(It is run inside an END block.)  Besides, C<onfail> is probably
over-kill in the majority of cases.  (Your test code should be simpler
than the code it is testing, yes?)

=head1 SEE ALSO

L<Test::Harness> and various test coverage analysis tools.

=head1 AUTHOR

Copyright (C) 1998 Joshua Nathaniel Pritikin.  All rights reserved.

This package is free software and is provided "as is" without express
or implied warranty.  It may be used, redistributed and/or modified
under the terms of the Perl Artistic License (see
http://www.perl.com/perl/misc/Artistic.html)

=cut
