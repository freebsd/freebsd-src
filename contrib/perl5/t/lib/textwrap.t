#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

@tests = (split(/\nEND\n/s, <<DONE));
TEST1
This 
is
a
test
END
   This 
 is
 a
 test
END
TEST2
This is a test of a very long line.  It should be broken up and put onto multiple lines.
This is a test of a very long line.  It should be broken up and put onto multiple lines.

This is a test of a very long line.  It should be broken up and put onto multiple lines.
END
   This is a test of a very long line.	It should be broken up and put onto
 multiple lines.
 This is a test of a very long line.  It should be broken up and put onto
 multiple lines.
 
 This is a test of a very long line.  It should be broken up and put onto
 multiple lines.
END
TEST3
This is a test of a very long line.  It should be broken up and put onto multiple lines.
END
   This is a test of a very long line.	It should be broken up and put onto
 multiple lines.
END
TEST4
This is a test of a very long line.  It should be broken up and put onto multiple lines.

END
   This is a test of a very long line.	It should be broken up and put onto
 multiple lines.

END
TEST5
This is a test of a very long line. It should be broken up and put onto multiple This is a test of a very long line. It should be broken up and put
END
   This is a test of a very long line. It should be broken up and put onto
 multiple This is a test of a very long line. It should be broken up and
 put
END
TEST6
11111111 22222222 33333333 44444444 55555555 66666666 77777777 888888888 999999999 aaaaaaaaa bbbbbbbbb ccccccccc ddddddddd eeeeeeeee ffffffff gggggggg hhhhhhhh iiiiiiii jjjjjjjj kkkkkkkk llllllll mmmmmmmmm nnnnnnnnn ooooooooo ppppppppp qqqqqqqqq rrrrrrrrr sssssssss
END
   11111111 22222222 33333333 44444444 55555555 66666666 77777777 888888888
 999999999 aaaaaaaaa bbbbbbbbb ccccccccc ddddddddd eeeeeeeee ffffffff
 gggggggg hhhhhhhh iiiiiiii jjjjjjjj kkkkkkkk llllllll mmmmmmmmm nnnnnnnnn
 ooooooooo ppppppppp qqqqqqqqq rrrrrrrrr sssssssss
END
TEST7
c3t1d0s6 c4t1d0s6 c5t1d0s6 c6t1d0s6 c7t1d0s6 c8t1d0s6 c9t1d0s6 c10t1d0s6 c11t1d0s6 c12t1d0s6 c13t1d0s6 c14t1d0s6 c15t1d0s6 c16t1d0s6 c3t1d0s0 c4t1d0s0 c5t1d0s0 c6t1d0s0 c7t1d0s0 c8t1d0s0 c9t1d0s0 c10t1d0s0 c11t1d0s0 c12t1d0s0 c13t1d0s0 c14t1d0s0 c15t1d0s0 c16t1d0s0
END
   c3t1d0s6 c4t1d0s6 c5t1d0s6 c6t1d0s6 c7t1d0s6 c8t1d0s6 c9t1d0s6 c10t1d0s6
 c11t1d0s6 c12t1d0s6 c13t1d0s6 c14t1d0s6 c15t1d0s6 c16t1d0s6 c3t1d0s0
 c4t1d0s0 c5t1d0s0 c6t1d0s0 c7t1d0s0 c8t1d0s0 c9t1d0s0 c10t1d0s0 c11t1d0s0
 c12t1d0s0 c13t1d0s0 c14t1d0s0 c15t1d0s0 c16t1d0s0
END
TEST8
A test of a very very long word.
a123456789b123456789c123456789d123456789e123456789f123456789g123456789g1234567
END
   A test of a very very long word.
 a123456789b123456789c123456789d123456789e123456789f123456789g123456789g123
 4567
END
TEST9
A test of a very very long word.  a123456789b123456789c123456789d123456789e123456789f123456789g123456789g1234567
END
   A test of a very very long word. 
 a123456789b123456789c123456789d123456789e123456789f123456789g123456789g123
 4567
END
TEST10
my mother once said
"never eat paste my darling"
would that I heeded
END
   my mother once said
 "never eat paste my darling"
 would that I heeded
END
TEST11
This_is_a_word_that_is_too_long_to_wrap_we_want_to_make_sure_that_the_program_does_not_crash_and_burn
END
   This_is_a_word_that_is_too_long_to_wrap_we_want_to_make_sure_that_the_pr
 ogram_does_not_crash_and_burn
END
TEST12
This

Has

Blank

Lines

END
   This
 
 Has
 
 Blank
 
 Lines

END
DONE


$| = 1;

print "1..", 1 +@tests, "\n";

use Text::Wrap;

$rerun = $ENV{'PERL_DL_NONLAZY'} ? 0 : 1;

$tn = 1;

@st = @tests;
while (@st) {
	my $in = shift(@st);
	my $out = shift(@st);

	$in =~ s/^TEST(\d+)?\n//;

	my $back = wrap('   ', ' ', $in);

	if ($back eq $out) {
		print "ok $tn\n";
	} elsif ($rerun) {
		my $oi = $in;
		foreach ($in, $back, $out) {
			s/\t/^I\t/gs;
			s/\n/\$\n/gs;
		}
		print "------------ input ------------\n";
		print $in;
		print "\n------------ output -----------\n";
		print $back;
		print "\n------------ expected ---------\n";
		print $out;
		print "\n-------------------------------\n";
		$Text::Wrap::debug = 1;
		wrap('   ', ' ', $oi);
		exit(1);
	} else {
		print "not ok $tn\n";
	}
	$tn++;

}

@st = @tests;
while(@st) {
	my $in = shift(@st);
	my $out = shift(@st);

	$in =~ s/^TEST(\d+)?\n//;

	my @in = split("\n", $in, -1);
	@in = ((map { "$_\n" } @in[0..$#in-1]), $in[-1]);
	
	my $back = wrap('   ', ' ', @in);

	if ($back eq $out) {
		print "ok $tn\n";
	} elsif ($rerun) {
		my $oi = $in;
		foreach ($in, $back, $out) {
			s/\t/^I\t/gs;
			s/\n/\$\n/gs;
		}
		print "------------ input2 ------------\n";
		print $in;
		print "\n------------ output2 -----------\n";
		print $back;
		print "\n------------ expected2 ---------\n";
		print $out;
		print "\n-------------------------------\n";
		$Text::Wrap::debug = 1;
		wrap('   ', ' ', $oi);
		exit(1);
	} else {
		print "not ok $tn\n";
	}
	$tn++;
}

$Text::Wrap::huge = 'overflow';

my $tw = 'This_is_a_word_that_is_too_long_to_wrap_we_want_to_make_sure_that_the_program_does_not_crash_and_burn';
my $w = wrap('zzz','yyy',$tw);
print (($w eq "zzz$tw") ? "ok $tn\n" : "not ok $tn");
$tn++;

