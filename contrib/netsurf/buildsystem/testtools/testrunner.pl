#!/bin/perl
#
# Testcase runner for libhubbub
#
# Usage: testrunner <builddir> <testdir> <prefix> [<executable extension>]
#
# Operates upon INDEX files described in the README.
# Locates and executes testcases, feeding data files to programs 
# as appropriate.
# Logs testcase output to file.
# Aborts test sequence on detection of error.
#

use warnings;
use strict;
use File::Spec;
use IO::Select;
use IPC::Open3;
use Symbol;

if (@ARGV < 3) {
	print "Usage: testrunner.pl <builddir> <testdir> <prefix> [<exeext>]\n";
	exit;
}

# Get directories
my $builddir = shift @ARGV;
my $directory = shift @ARGV;
my $prefix = shift @ARGV;

# Get EXE extension (if any)
my $exeext = "";
$exeext = shift @ARGV if (@ARGV > 0);

# Open log file and /dev/null
open(LOG, ">$builddir/testlog") or die "Failed opening test log";
open(NULL, "+<", File::Spec->devnull) or die "Failed opening /dev/null";

# Open testcase index
open(TINDEX, "<$directory/INDEX") or die "Failed opening test INDEX";

# Parse testcase index, looking for testcases
while (my $line = <TINDEX>) {
	next if ($line =~ /^(#.*)?$/);

	# Found one; decompose
	(my $test, my $desc, my $data) = split /\t+/, $line;

	# Strip whitespace
	$test =~ s/^\s+|\s+$//g;
	$desc =~ s/^\s+|\s+$//g;
	$data =~ s/^\s+|\s+$//g if ($data);

	# Append prefix & EXE extension to binary name
	$test = $prefix . $test . $exeext;

	print "Test: $desc\n";

	if ($data) {
		# Testcase has external data files

		# Open datafile index
		open(DINDEX, "<$directory/data/$data/INDEX") or 
			die "Failed opening $directory/data/$data/INDEX";

		# Parse datafile index, looking for datafiles
		while (my $dentry = <DINDEX>) {
			next if ($dentry =~ /^(#.*)?$/);

			# Found one; decompose
			(my $dtest, my $ddesc) = split /\t+/, $dentry;

			# Strip whitespace
			$dtest =~ s/^\s+|\s+$//g;
			$ddesc =~ s/^\s+|\s+$//g;

			print LOG "Running $builddir/$test " .
					"$directory/data/$data/$dtest\n";

			# Make message fit on an 80 column terminal
			my $msg = "    ==> $test [$data/$dtest]";
			$msg = $msg . "." x (80 - length($msg) - 8);

			print $msg;

			# Run testcase
			run_test("$builddir/$test", 
					"$directory/data/$data/$dtest");
                }

		close(DINDEX);
	} else {
		# Testcase has no external data files
		print LOG "Running $builddir/$test\n";

		# Make message fit on an 80 column terminal
		my $msg = "    ==> $test";
		$msg = $msg . "." x (80 - length($msg) - 8);

		print $msg;

		# Run testcase
		run_test("$builddir/$test");
	}

	print "\n";
}

# Clean up
close(TINDEX);

close(NULL);
close(LOG);

sub run_test
{
	my @errors;

	# Handles for communicating with the child
	my ($out, $err);
	$err = gensym(); # Apparently, this is required

	my $pid;

	# Invoke child
	eval {
		$pid = open3("&<NULL", $out, $err, @_);
	};
	die $@ if $@;

	my $selector = IO::Select->new();
	$selector->add($out, $err);

	my $last = "FAIL";
	my $outcont = 0;
	my $errcont = 0;

	# Marshal testcase output to log file
	while (my @ready = $selector->can_read) {
		foreach my $fh (@ready) {
			my $input;
			# Read up to 4096 bytes from handle
			my $len = sysread($fh, $input, 4096);

			if (!defined $len) {
				die "Error reading from child: $!\n";
			} elsif ($len == 0) {
				# EOF, so remove handle
				$selector->remove($fh);
				next;
			} else {
				# Split into lines
				my @lines = split('\n', $input);

				# Grab the last character of the input
				my $lastchar = substr($input, -1, 1);

				if ($fh == $out) {
					# Child's stdout
					foreach my $l (@lines) {
						# Last line of previous read
						# was incomplete, and this is
						# the first line of this read
						# Simply contatenate.
						if ($outcont == 1 && 
							$l eq $lines[0]) {
							print LOG "$l\n";
							$last .= $l;
						# Last char of this read was 
						# not '\n', so don't terminate
						# line in log.
						} elsif ($lastchar ne '\n' &&
							$l eq $lines[-1]) {
							print LOG "    $l";
							$last = $l;
						# Normal behaviour, just print
						# the line to the log.
						} else {
							print LOG "    $l\n";
							$last = $l;
						}
					}

					# Flag whether last line was incomplete
					# for next time.
					if ($lastchar ne '\n') {
						$outcont = 1;
					} else {
						$outcont = 0;
					}
				} elsif ($fh == $err) {
					# Child's stderr
					if ($errcont == 1) {
						# Continuation required,
						# concatenate first line of 
						# this read with last of 
						# previous, then append the 
						# rest from this read.
						$errors[-1] .= $lines[0];
						push(@errors, @lines[1 .. -1]);
					} else {
						# Normal behaviour, just append
						push(@errors, @lines);
					}

					# Flag need for continuation
					if ($lastchar ne '\n') {
						$errcont = 1;
					} else {
						$errcont = 0;
					}
				} else {
					die "Unexpected file handle\n";
				}
			}
		}
	}

	# Last line of child's output may not be terminated, so ensure it
	# is in the log, for readability.
	print LOG "\n";

	# Reap child
	waitpid($pid, 0);

	# Catch non-zero exit status and turn it into failure
	if ($? != 0) {
		my $status = $? & 127;

		if ($status != 0) {
			print LOG "    FAIL: Exit status $status\n";
		}
		$last = "FAIL";
	}

	# Only interested in first 4 characters of last line
	$last = substr($last, 0, 4);

	# Convert all non-pass to fail
	if ($last ne "PASS") {
		$last = "FAIL";
	}

	print "$last\n";

	# Bail, noisily, on failure
	if ($last eq "FAIL") {
		# Write any stderr output to the log
		foreach my $error (@errors) {
			print LOG "    $error\n";
		}

		print "\n\nFailure detected: consult log file\n\n\n";

		exit(1);
	}
}

