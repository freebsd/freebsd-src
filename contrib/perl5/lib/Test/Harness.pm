package Test::Harness;

use 5.005_64;
use Exporter;
use Benchmark;
use Config;
use FileHandle;
use strict;

our($VERSION, $verbose, $switches, $have_devel_corestack, $curtest,
    $columns, @ISA, @EXPORT, @EXPORT_OK);
$have_devel_corestack = 0;

$VERSION = "1.1604";

$ENV{HARNESS_ACTIVE} = 1;

# Some experimental versions of OS/2 build have broken $?
my $ignore_exitcode = $ENV{HARNESS_IGNORE_EXITCODE};

my $files_in_dir = $ENV{HARNESS_FILELEAK_IN_DIR};

my $tests_skipped = 0;
my $subtests_skipped = 0;

@ISA=('Exporter');
@EXPORT= qw(&runtests);
@EXPORT_OK= qw($verbose $switches);

$verbose = 0;
$switches = "-w";
$columns = $ENV{HARNESS_COLUMNS} || $ENV{COLUMNS} || 80;

sub globdir { opendir DIRH, shift; my @f = readdir DIRH; closedir DIRH; @f }

sub runtests {
    my(@tests) = @_;
    local($|) = 1;
    my($test,$te,$ok,$next,$max,$pct,$totbonus,@failed,%failedtests);
    my $totmax = 0;
    my $totok = 0;
    my $files = 0;
    my $bad = 0;
    my $good = 0;
    my $total = @tests;

    # pass -I flags to children
    my $old5lib = $ENV{PERL5LIB};

    # VMS has a 255-byte limit on the length of %ENV entries, so
    # toss the ones that involve perl_root, the install location
    # for VMS
    my $new5lib;
    if ($^O eq 'VMS') {
	$new5lib = join($Config{path_sep}, grep {!/perl_root/i;} @INC);
	$switches =~ s/-(\S*[A-Z]\S*)/"-$1"/g;
    }
    else {
        $new5lib = join($Config{path_sep}, @INC);
    }

    local($ENV{'PERL5LIB'}) = $new5lib;

    my @dir_files = globdir $files_in_dir if defined $files_in_dir;
    my $t_start = new Benchmark;
    while ($test = shift(@tests)) {
	$te = $test;
	chop($te);
	if ($^O eq 'VMS') { $te =~ s/^.*\.t\./[.t./s; }
	my $blank = (' ' x 77);
	my $leader = "$te" . '.' x (20 - length($te));
	my $ml = "";
	$ml = "\r$blank\r$leader"
	    if -t STDOUT and not $ENV{HARNESS_NOTTY} and not $verbose;
	print $leader;
	my $fh = new FileHandle;
	$fh->open($test) or print "can't open $test. $!\n";
	my $first = <$fh>;
	my $s = $switches;
	$s .= " $ENV{'HARNESS_PERL_SWITCHES'}"
	    if exists $ENV{'HARNESS_PERL_SWITCHES'};
	$s .= join " ", q[ "-T"], map {qq["-I$_"]} @INC
	    if $first =~ /^#!.*\bperl.*-\w*T/;
	$fh->close or print "can't close $test. $!\n";
	my $cmd = ($ENV{'HARNESS_COMPILE_TEST'})
		? "./perl -I../lib ../utils/perlcc $test "
		  . "-r 2>> ./compilelog |" 
		: "$^X $s $test|";
	$cmd = "MCR $cmd" if $^O eq 'VMS';
	$fh->open($cmd) or print "can't run $test. $!\n";
	$ok = $next = $max = 0;
	@failed = ();
	my %todo = ();
        my $bonus = 0;
	my $skipped = 0;
	my $skip_reason;
	while (<$fh>) {
	    if( $verbose ){
		print $_;
	    }
	    if (/^1\.\.([0-9]+) todo([\d\s]+)\;/) {
		$max = $1;
		for (split(/\s+/, $2)) { $todo{$_} = 1; }
		$totmax += $max;
		$files++;
		$next = 1;
	    } elsif (/^1\.\.([0-9]+)(\s*\#\s*[Ss]kip\S*(?>\s+)(.+))?/) {
		$max = $1;
		$totmax += $max;
		$files++;
		$next = 1;
		$skip_reason = $3 if not $max and defined $3;
	    } elsif ($max && /^(not\s+)?ok\b/) {
		my $this = $next;
		if (/^not ok\s*(\d*)/){
		    $this = $1 if $1 > 0;
		    print "${ml}NOK $this" if $ml;
		    if (!$todo{$this}) {
			push @failed, $this;
		    } else {
			$ok++;
			$totok++;
		    }
		} elsif (/^ok\s*(\d*)(\s*\#\s*[Ss]kip\S*(?:(?>\s+)(.+))?)?/) {
		    $this = $1 if $1 > 0;
		    print "${ml}ok $this/$max" if $ml;
		    $ok++;
		    $totok++;
		    $skipped++ if defined $2;
		    my $reason;
		    $reason = 'unknown reason' if defined $2;
		    $reason = $3 if defined $3;
		    if (defined $reason and defined $skip_reason) {
		      # print "was: '$skip_reason' new '$reason'\n";
		      $skip_reason = 'various reasons'
			if $skip_reason ne $reason;
		    } elsif (defined $reason) {
		      $skip_reason = $reason;
		    }
		    $bonus++, $totbonus++ if $todo{$this};
		}
		if ($this > $next) {
		    # print "Test output counter mismatch [test $this]\n";
		    # no need to warn probably
		    push @failed, $next..$this-1;
		} elsif ($this < $next) {
		    #we have seen more "ok" lines than the number suggests
		    print "Confused test output: test $this answered after test ", $next-1, "\n";
		    $next = $this;
		}
		$next = $this + 1;
	    }
	}
	$fh->close; # must close to reap child resource values
	my $wstatus = $ignore_exitcode ? 0 : $?;	# Can trust $? ?
	my $estatus;
	$estatus = ($^O eq 'VMS'
		       ? eval 'use vmsish "status"; $estatus = $?'
		       : $wstatus >> 8);
	if ($wstatus) {
	    my ($failed, $canon, $percent) = ('??', '??');
	    printf "${ml}dubious\n\tTest returned status $estatus (wstat %d, 0x%x)\n",
		    $wstatus,$wstatus;
	    print "\t\t(VMS status is $estatus)\n" if $^O eq 'VMS';
	    if (corestatus($wstatus)) { # until we have a wait module
		if ($have_devel_corestack) {
		    Devel::CoreStack::stack($^X);
		} else {
		    print "\ttest program seems to have generated a core\n";
		}
	    }
	    $bad++;
	    if ($max) {
	      if ($next == $max + 1 and not @failed) {
		print "\tafter all the subtests completed successfully\n";
		$percent = 0;
		$failed = 0;	# But we do not set $canon!
	      } else {
		push @failed, $next..$max;
		$failed = @failed;
		(my $txt, $canon) = canonfailed($max,$skipped,@failed);
		$percent = 100*(scalar @failed)/$max;
		print "DIED. ",$txt;
	      }
	    }
	    $failedtests{$test} = { canon => $canon,  max => $max || '??',
				    failed => $failed, 
				    name => $test, percent => $percent,
				    estat => $estatus, wstat => $wstatus,
				  };
	} elsif ($ok == $max && $next == $max+1) {
	    if ($max and $skipped + $bonus) {
		my @msg;
		push(@msg, "$skipped/$max skipped: $skip_reason")
		    if $skipped;
		push(@msg, "$bonus/$max unexpectedly succeeded")
		    if $bonus;
		print "${ml}ok, ".join(', ', @msg)."\n";
	    } elsif ($max) {
		print "${ml}ok\n";
	    } elsif (defined $skip_reason) {
		print "skipped: $skip_reason\n";
		$tests_skipped++;
	    } else {
		print "skipped test on this platform\n";
		$tests_skipped++;
	    }
	    $good++;
	} elsif ($max) {
	    if ($next <= $max) {
		push @failed, $next..$max;
	    }
	    if (@failed) {
		my ($txt, $canon) = canonfailed($max,$skipped,@failed);
		print "${ml}$txt";
		$failedtests{$test} = { canon => $canon,  max => $max,
					failed => scalar @failed,
					name => $test, percent => 100*(scalar @failed)/$max,
					estat => '', wstat => '',
				      };
	    } else {
		print "Don't know which tests failed: got $ok ok, expected $max\n";
		$failedtests{$test} = { canon => '??',  max => $max,
					failed => '??', 
					name => $test, percent => undef,
					estat => '', wstat => '',
				      };
	    }
	    $bad++;
	} elsif ($next == 0) {
	    print "FAILED before any test output arrived\n";
	    $bad++;
	    $failedtests{$test} = { canon => '??',  max => '??',
				    failed => '??',
				    name => $test, percent => undef,
				    estat => '', wstat => '',
				  };
	}
	$subtests_skipped += $skipped;
	if (defined $files_in_dir) {
	    my @new_dir_files = globdir $files_in_dir;
	    if (@new_dir_files != @dir_files) {
		my %f;
		@f{@new_dir_files} = (1) x @new_dir_files;
		delete @f{@dir_files};
		my @f = sort keys %f;
		print "LEAKED FILES: @f\n";
		@dir_files = @new_dir_files;
	    }
	}
    }
    my $t_total = timediff(new Benchmark, $t_start);
    
    if ($^O eq 'VMS') {
	if (defined $old5lib) {
	    $ENV{PERL5LIB} = $old5lib;
	} else {
	    delete $ENV{PERL5LIB};
	}
    }
    my $bonusmsg = '';
    $bonusmsg = (" ($totbonus subtest".($totbonus>1?'s':'').
	       " UNEXPECTEDLY SUCCEEDED)")
	if $totbonus;
    if ($tests_skipped) {
	$bonusmsg .= ", $tests_skipped test" . ($tests_skipped != 1 ? 's' : '');
	if ($subtests_skipped) {
	    $bonusmsg .= " and $subtests_skipped subtest"
			 . ($subtests_skipped != 1 ? 's' : '');
	}
	$bonusmsg .= ' skipped';
    }
    elsif ($subtests_skipped) {
	$bonusmsg .= ", $subtests_skipped subtest"
	             . ($subtests_skipped != 1 ? 's' : '')
		     . " skipped";
    }
    if ($bad == 0 && $totmax) {
	print "All tests successful$bonusmsg.\n";
    } elsif ($total==0){
	die "FAILED--no tests were run for some reason.\n";
    } elsif ($totmax==0) {
	my $blurb = $total==1 ? "script" : "scripts";
	die "FAILED--$total test $blurb could be run, alas--no output ever seen\n";
    } else {
	$pct = sprintf("%.2f", $good / $total * 100);
	my $subpct = sprintf " %d/%d subtests failed, %.2f%% okay.",
	$totmax - $totok, $totmax, 100*$totok/$totmax;
	# Create formats
	#    First, figure out max length of test names
	my $failed_str = "Failed Test";
	my $middle_str = " Status Wstat Total Fail  Failed  ";
	my $list_str = "List of Failed";
	my $max_namelen = length($failed_str);
	my $script;
	foreach $script (keys %failedtests) {
	    $max_namelen =
		(length $failedtests{$script}->{name} > $max_namelen) ?
		    length $failedtests{$script}->{name} : $max_namelen;
	}
	my $list_len = $columns - length($middle_str) - $max_namelen;
	if ($list_len < length($list_str)) {
	    $list_len = length($list_str);
	    $max_namelen = $columns - length($middle_str) - $list_len;
	    if ($max_namelen < length($failed_str)) {
		$max_namelen = length($failed_str);
		$columns = $max_namelen + length($middle_str) + $list_len;
	    }
	}

	my $fmt_top = "format STDOUT_TOP =\n"
		      . sprintf("%-${max_namelen}s", $failed_str)
		      . $middle_str
		      . $list_str . "\n"
		      . "-" x $columns
		      . "\n.\n";
	my $fmt = "format STDOUT =\n"
		  . "@" . "<" x ($max_namelen - 1)
		  . "	 @>> @>>>> @>>>> @>>> ^##.##%  "
		  . "^" . "<" x ($list_len - 1) . "\n"
		  . '{ $curtest->{name}, $curtest->{estat},'
		  . '  $curtest->{wstat}, $curtest->{max},'
		  . '  $curtest->{failed}, $curtest->{percent},'
		  . '  $curtest->{canon}'
		  . "\n}\n"
		  . "~~" . " " x ($columns - $list_len - 2) . "^"
		  . "<" x ($list_len - 1) . "\n"
		  . '$curtest->{canon}'
		  . "\n.\n";

	eval $fmt_top;
	die $@ if $@;
	eval $fmt;
	die $@ if $@;

	# Now write to formats
	for $script (sort keys %failedtests) {
	  $curtest = $failedtests{$script};
	  write;
	}
	if ($bad) {
	    $bonusmsg =~ s/^,\s*//;
	    print "$bonusmsg.\n" if $bonusmsg;
	    die "Failed $bad/$total test scripts, $pct% okay.$subpct\n";
	}
    }
    printf("Files=%d, Tests=%d, %s\n", $files, $totmax, timestr($t_total, 'nop'));

    return ($bad == 0 && $totmax) ;
}

my $tried_devel_corestack;
sub corestatus {
    my($st) = @_;

    eval {require 'wait.ph'};
    my $ret = defined &WCOREDUMP ? WCOREDUMP($st) : $st & 0200;

    eval { require Devel::CoreStack; $have_devel_corestack++ } 
      unless $tried_devel_corestack++;

    $ret;
}

sub canonfailed ($@) {
    my($max,$skipped,@failed) = @_;
    my %seen;
    @failed = sort {$a <=> $b} grep !$seen{$_}++, @failed;
    my $failed = @failed;
    my @result = ();
    my @canon = ();
    my $min;
    my $last = $min = shift @failed;
    my $canon;
    if (@failed) {
	for (@failed, $failed[-1]) { # don't forget the last one
	    if ($_ > $last+1 || $_ == $last) {
		if ($min == $last) {
		    push @canon, $last;
		} else {
		    push @canon, "$min-$last";
		}
		$min = $_;
	    }
	    $last = $_;
	}
	local $" = ", ";
	push @result, "FAILED tests @canon\n";
	$canon = "@canon";
    } else {
	push @result, "FAILED test $last\n";
	$canon = $last;
    }

    push @result, "\tFailed $failed/$max tests, ";
    push @result, sprintf("%.2f",100*(1-$failed/$max)), "% okay";
    my $ender = 's' x ($skipped > 1);
    my $good = $max - $failed - $skipped;
    my $goodper = sprintf("%.2f",100*($good/$max));
    push @result, " (-$skipped skipped test$ender: $good okay, $goodper%)" if $skipped;
    push @result, "\n";
    my $txt = join "", @result;
    ($txt, $canon);
}

1;
__END__

=head1 NAME

Test::Harness - run perl standard test scripts with statistics

=head1 SYNOPSIS

use Test::Harness;

runtests(@tests);

=head1 DESCRIPTION

(By using the L<Test> module, you can write test scripts without
knowing the exact output this module expects.  However, if you need to
know the specifics, read on!)

Perl test scripts print to standard output C<"ok N"> for each single
test, where C<N> is an increasing sequence of integers. The first line
output by a standard test script is C<"1..M"> with C<M> being the
number of tests that should be run within the test
script. Test::Harness::runtests(@tests) runs all the testscripts
named as arguments and checks standard output for the expected
C<"ok N"> strings.

After all tests have been performed, runtests() prints some
performance statistics that are computed by the Benchmark module.

=head2 The test script output

Any output from the testscript to standard error is ignored and
bypassed, thus will be seen by the user. Lines written to standard
output containing C</^(not\s+)?ok\b/> are interpreted as feedback for
runtests().  All other lines are discarded.

It is tolerated if the test numbers after C<ok> are omitted. In this
case Test::Harness maintains temporarily its own counter until the
script supplies test numbers again. So the following test script

    print <<END;
    1..6
    not ok
    ok
    not ok
    ok
    ok
    END

will generate 

    FAILED tests 1, 3, 6
    Failed 3/6 tests, 50.00% okay

The global variable $Test::Harness::verbose is exportable and can be
used to let runtests() display the standard output of the script
without altering the behavior otherwise.

The global variable $Test::Harness::switches is exportable and can be
used to set perl command line options used for running the test
script(s). The default value is C<-w>.

If the standard output line contains substring C< # Skip> (with
variations in spacing and case) after C<ok> or C<ok NUMBER>, it is
counted as a skipped test.  If the whole testscript succeeds, the
count of skipped tests is included in the generated output.

C<Test::Harness> reports the text after C< # Skip(whatever)> as a
reason for skipping.  Similarly, one can include a similar explanation
in a C<1..0> line emitted if the test is skipped completely:

  1..0 # Skipped: no leverage found

=head1 EXPORT

C<&runtests> is exported by Test::Harness per default.

=head1 DIAGNOSTICS

=over 4

=item C<All tests successful.\nFiles=%d,  Tests=%d, %s>

If all tests are successful some statistics about the performance are
printed.

=item C<FAILED tests %s\n\tFailed %d/%d tests, %.2f%% okay.>

For any single script that has failing subtests statistics like the
above are printed.

=item C<Test returned status %d (wstat %d)>

Scripts that return a non-zero exit status, both C<$? E<gt>E<gt> 8> and C<$?> are
printed in a message similar to the above.

=item C<Failed 1 test, %.2f%% okay. %s>

=item C<Failed %d/%d tests, %.2f%% okay. %s>

If not all tests were successful, the script dies with one of the
above messages.

=back

=head1 ENVIRONMENT

Setting C<HARNESS_IGNORE_EXITCODE> makes harness ignore the exit status
of child processes.

Setting C<HARNESS_NOTTY> to a true value forces it to behave as though
STDOUT were not a console.  You may need to set this if you don't want
harness to output more frequent progress messages using carriage returns.
Some consoles may not handle carriage returns properly (which results
in a somewhat messy output).

Setting C<HARNESS_COMPILE_TEST> to a true value will make harness attempt
to compile the test using C<perlcc> before running it.

If C<HARNESS_FILELEAK_IN_DIR> is set to the name of a directory, harness
will check after each test whether new files appeared in that directory,
and report them as

  LEAKED FILES: scr.tmp 0 my.db

If relative, directory name is with respect to the current directory at
the moment runtests() was called.  Putting absolute path into 
C<HARNESS_FILELEAK_IN_DIR> may give more predicatable results.

The value of C<HARNESS_PERL_SWITCHES> will be prepended to the
switches used to invoke perl on each test.  For example, setting
C<HARNESS_PERL_SWITCHES> to "-W" will run all tests with all
warnings enabled.

If C<HARNESS_COLUMNS> is set, then this value will be used for the
width of the terminal. If it is not set then it will default to
C<COLUMNS>. If this is not set, it will default to 80. Note that users
of Bourne-sh based shells will need to C<export COLUMNS> for this
module to use that variable.

Harness sets C<HARNESS_ACTIVE> before executing the individual tests.
This allows the tests to determine if they are being executed through the
harness or by any other means.

=head1 SEE ALSO

L<Test> for writing test scripts and also L<Benchmark> for the
underlying timing routines.

=head1 AUTHORS

Either Tim Bunce or Andreas Koenig, we don't know. What we know for
sure is, that it was inspired by Larry Wall's TEST script that came
with perl distributions for ages. Numerous anonymous contributors
exist. Current maintainer is Andreas Koenig.

=head1 BUGS

Test::Harness uses $^X to determine the perl binary to run the tests
with. Test scripts running via the shebang (C<#!>) line may not be
portable because $^X is not consistent for shebang scripts across
platforms. This is no problem when Test::Harness is run with an
absolute path to the perl binary or when $^X can be found in the path.

=cut
