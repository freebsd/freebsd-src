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
DONE


$| = 1;

print "1..", @tests/2, "\n";

use Text::Wrap;

$rerun = $ENV{'PERL_DL_NONLAZY'} ? 0 : 1;

$tn = 1;
while (@tests) {
	my $in = shift(@tests);
	my $out = shift(@tests);

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
