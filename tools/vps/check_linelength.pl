#!/usr/bin/perl -w

use strict;

exit(main(\@ARGV));

sub main {
	my $line;
	my $length;
	my $cpos;
	my $lpos;
	my $spacescnt;
	my $spacesstr;
	my $argv = shift;
	my $linenum = 0;

	#print("argv[0]=@$argv[0] argv[1]=@$argv[1]\n");

	open(FILE, "<", @$argv[0]) or die "cannot open [@$argv[0]]";

	while (<FILE>) {
		$line = $_;

		$linenum += 1;

		# Convert '\t' to '        '.
		#$line =~ s/\t/        /g;
		$lpos = 0;
		while ($line =~ m/\t/) {
			$cpos = index($line, "\t", $lpos);
			$spacescnt = 8 - ($cpos % 8);
			#print("cpos=$cpos spacescnt=$spacescnt\n");
			$spacesstr = "";
			while ($spacescnt > 0) {
				$spacesstr .= " ";
				$spacescnt -= 1;
			}
			$line =~ s/\t/$spacesstr/;
			$lpos = $cpos;
		}

		# Remove '\n'.
		$line =~ s/\n//;

		#print("$line\n");

		$length = length($line);

		if ($length > 76) {
			print("line $linenum length $length [$line]\n");
		}
	}

	close(FILE);

	return (0);
}

# EOF
