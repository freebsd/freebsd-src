#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

@tests = (split(/\nEND\n/s, <<DONE));
TEST 1 u
                x
END
		x
END
TEST 2 e
		x
END
                x
END
TEST 3 e
	x
		y
			z
END
        x
                y
                        z
END
TEST 4 u
        x
                y
                        z
END
	x
		y
			z
END
TEST 5 u
This    Is      a       test    of      a       line with many embedded tabs
END
This	Is	a	test	of	a	line with many embedded tabs
END
TEST 6 e
This	Is	a	test	of	a	line with many embedded tabs
END
This    Is      a       test    of      a       line with many embedded tabs
END
TEST 7 u
            x
END
	    x
END
TEST 8 e
	
		
   	

           
END
        
                
        

           
END
TEST 9 u
           
END
	   
END
TEST 10 u
	
		
   	

           
END
	
		
	

	   
END
TEST 11 u
foobar                  IN	A		140.174.82.12

END
foobar			IN	A		140.174.82.12

END
DONE

$| = 1;

print "1..".scalar(@tests/2)."\n";

use Text::Tabs;

$rerun = $ENV{'PERL_DL_NONLAZY'} ? 0 : 1;

$tn = 1;
while (@tests) {
	my $in = shift(@tests);
	my $out = shift(@tests);

	$in =~ s/^TEST\s*(\d+)?\s*(\S+)?\n//;

	if ($2 eq 'e') {
		$f = \&expand;
		$fn = 'expand';
	} else {
		$f = \&unexpand;
		$fn = 'unexpand';
	}

	my $back = &$f($in);

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
		print "\$\n------------ $fn -----------\n";
		print $back;
		print "\$\n------------ expected ---------\n";
		print $out;
		print "\$\n-------------------------------\n";
		$Text::Tabs::debug = 1;
		my $back = &$f($in);
		exit(1);
	} else {
		print "not ok $tn\n";
	}
	$tn++;
}
