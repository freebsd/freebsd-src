use strict;
package Test;
use Test::Harness 1.1601 ();
use Carp;
our($VERSION, @ISA, @EXPORT, @EXPORT_OK, $ntest, $TestLevel); #public-ish
our($TESTOUT, $ONFAIL, %todo, %history, $planned, @FAILDETAIL); #private-ish
$VERSION = '1.15';
require Exporter;
@ISA=('Exporter');
@EXPORT=qw(&plan &ok &skip);
@EXPORT_OK=qw($ntest $TESTOUT);

$TestLevel = 0;		# how many extra stack frames to skip
$|=1;
#$^W=1;  ?
$ntest=1;
$TESTOUT = *STDOUT{IO};

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
	print $TESTOUT "1..$max todo ".join(' ', @todo).";\n";
    } else {
	print $TESTOUT "1..$max\n";
    }
    ++$planned;
}

sub to_value {
    my ($v) = @_;
    (ref $v or '') eq 'CODE' ? $v->() : $v;
}

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
	my ($regex,$ignore);
	if (!defined $expected) {
	    $ok = !defined $result;
	} elsif (!defined $result) {
	    $ok = 0;
	} elsif ((ref($expected)||'') eq 'Regexp') {
	    $ok = $result =~ /$expected/;
	} elsif (($regex) = ($expected =~ m,^ / (.+) / $,sx) or
	    ($ignore, $regex) = ($expected =~ m,^ m([^\w\s]) (.+) \1 $,sx)) {
	    $ok = $result =~ /$regex/;
	} else {
	    $ok = $result eq $expected;
	}
    }
    my $todo = $todo{$ntest};
    if ($todo and $ok) {
	$context .= ' TODO?!' if $todo;
	print $TESTOUT "ok $ntest # ($context)\n";
    } else {
	# Issuing two separate print()s causes severe trouble with 
	# Test::Harness on VMS.  The "not "'s for failed tests occur
	# on a separate line and would not get counted as failures.
	#print $TESTOUT "not " if !$ok;
	#print $TESTOUT "ok $ntest\n";
	# Replace with a single print() as a workaround:
	my $okline = '';
	$okline = "not " if !$ok;
	$okline .= "ok $ntest\n";
	print $TESTOUT $okline;
	
	if (!$ok) {
	    my $detail = { 'repetition' => $repetition, 'package' => $pkg,
			   'result' => $result, 'todo' => $todo };
	    $$detail{expected} = $expected if defined $expected;
	    $diag = $$detail{diagnostic} = to_value(shift) if @_;
	    $context .= ' *TODO*' if $todo;
	    if (!defined $expected) {
		if (!$diag) {
		    print $TESTOUT "# Failed test $ntest in $context\n";
		} else {
		    print $TESTOUT "# Failed test $ntest in $context: $diag\n";
		}
	    } else {
		my $prefix = "Test $ntest";
		print $TESTOUT "# $prefix got: ".
		    (defined $result? "'$result'":'<UNDEF>')." ($context)\n";
		$prefix = ' ' x (length($prefix) - 5);
		if ((ref($expected)||'') eq 'Regexp') {
		    $expected = 'qr/'.$expected.'/'
		} else {
		    $expected = "'$expected'";
		}
		if (!$diag) {
		    print $TESTOUT "# $prefix Expected: $expected\n";
		} else {
		    print $TESTOUT "# $prefix Expected: $expected ($diag)\n";
		}
	    }
	    push @FAILDETAIL, $detail;
	}
    }
    ++ $ntest;
    $ok;
}

sub skip ($$;$$) {
    my $whyskip = to_value(shift);
    if ($whyskip) {
	$whyskip = 'skip' if $whyskip =~ m/^\d+$/;
	print $TESTOUT "ok $ntest # $whyskip\n";
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

  # use a BEGIN block so we print our plan before MyModule is loaded
  BEGIN { plan tests => 14, todo => [3,4] }

  # load your module...
  use MyModule;

  ok(0); # failure
  ok(1); # success

  ok(0); # ok, expected failure (see todo list, above)
  ok(1); # surprise success!

  ok(0,1);             # failure: '0' ne '1'
  ok('broke','fixed'); # failure: 'broke' ne 'fixed'
  ok('fixed','fixed'); # success: 'fixed' eq 'fixed'
  ok('fixed',qr/x/);   # success: 'fixed' =~ qr/x/

  ok(sub { 1+1 }, 2);  # success: '2' eq '2'
  ok(sub { 1+1 }, 3);  # failure: '2' ne '3'
  ok(0, int(rand(2));  # (just kidding :-)

  my @list = (0,0);
  ok @list, 3, "\@list=".join(',',@list);      #extra diagnostics
  ok 'segmentation fault', '/(?i)success/';    #regex match

  skip($feature_is_missing, ...);    #do platform specific test

=head1 DESCRIPTION

L<Test::Harness|Test::Harness> expects to see particular output when it
executes tests.  This module aims to make writing proper test scripts just
a little bit easier (and less error prone :-).

=head1 TEST TYPES

=over 4

=item * NORMAL TESTS

These tests are expected to succeed.  If they don't something's
screwed up!

=item * SKIPPED TESTS

Skip is for tests that might or might not be possible to run depending
on the availability of platform specific features.  The first argument
should evaluate to true (think "yes, please skip") if the required
feature is not available.  After the first argument, skip works
exactly the same way as do normal tests.

=item * TODO TESTS

TODO tests are designed for maintaining an B<executable TODO list>.
These tests are expected NOT to succeed.  If a TODO test does succeed,
the feature in question should not be on the TODO list, now should it?

Packages should NOT be released with succeeding TODO tests.  As soon
as a TODO test starts working, it should be promoted to a normal test
and the newly working feature should be documented in the release
notes or change log.

=back

=head1 RETURN VALUE

Both C<ok> and C<skip> return true if their test succeeds and false
otherwise in a scalar context.

=head1 ONFAIL

  BEGIN { plan test => 4, onfail => sub { warn "CALL 911!" } }

While test failures should be enough, extra diagnostics can be
triggered at the end of a test run.  C<onfail> is passed an array ref
of hash refs that describe each test failure.  Each hash will contain
at least the following fields: C<package>, C<repetition>, and
C<result>.  (The file, line, and test number are not included because
their correspondence to a particular test is tenuous.)  If the test
had an expected value or a diagnostic string, these will also be
included.

The B<optional> C<onfail> hook might be used simply to print out the
version of your package and/or how to report problems.  It might also
be used to generate extremely sophisticated diagnostics for a
particularly bizarre test failure.  However it's not a panacea.  Core
dumps or other unrecoverable errors prevent the C<onfail> hook from
running.  (It is run inside an C<END> block.)  Besides, C<onfail> is
probably over-kill in most cases.  (Your test code should be simpler
than the code it is testing, yes?)

=head1 SEE ALSO

L<Test::Harness> and, perhaps, test coverage analysis tools.

=head1 AUTHOR

Copyright (c) 1998-1999 Joshua Nathaniel Pritikin.  All rights reserved.

This package is free software and is provided "as is" without express
or implied warranty.  It may be used, redistributed and/or modified
under the terms of the Perl Artistic License (see
http://www.perl.com/perl/misc/Artistic.html)

=cut
