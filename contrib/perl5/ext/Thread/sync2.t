use Thread;

$global = undef;

sub single_file : locked {
    my $who = shift;
    my $i;

    print "Uh oh: $who entered while locked by $global\n" if $global;
    $global = $who;
    print "[";
    for ($i = 0; $i < int(10 * rand); $i++) {
	print $who;
	select(undef, undef, undef, 0.1);
    }
    print "]";
    $global = undef;
}

sub start_a {
    my ($i, $j);
    for ($j = 0; $j < 10; $j++) {
	single_file("A");
	for ($i = 0; $i < int(10 * rand); $i++) {
	    print "a";
	    select(undef, undef, undef, 0.1);
	}
    }
}

sub start_b {
    my ($i, $j);
    for ($j = 0; $j < 10; $j++) {
	single_file("B");
	for ($i = 0; $i < int(10 * rand); $i++) {
	    print "b";
	    select(undef, undef, undef, 0.1);
	}
    }
}

sub start_c {
    my ($i, $j);
    for ($j = 0; $j < 10; $j++) {
	single_file("C");
	for ($i = 0; $i < int(10 * rand); $i++) {
	    print "c";
	    select(undef, undef, undef, 0.1);
	}
    }
}

$| = 1;
srand($$^$^T);

print <<'EOT';
Each pair of square brackets [...] should contain a repeated sequence of
a unique upper case letter. Lower case letters may appear randomly both
in and out of the brackets.
EOT
$foo = new Thread \&start_a;
$bar = new Thread \&start_b;
$baz = new Thread \&start_c;
print "\nmain: joining...\n";
#$foo->join;
#$bar->join;
#$baz->join;
print "\ndone\n";
