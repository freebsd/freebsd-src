sub foo {
	my $x;
	my $y;
	print "in sub foo\n";
	for( $x = 1; $x < 100; ++$x ){
		bar();
		for( $y = 1; $y < 100; ++$y ){
		}
	}
}

sub bar {
	my $x;
	print "in sub bar\n";
	for( $x = 1; $x < 100; ++$x ){
	}
	die "bar exiting";
}

sub baz {
	print "in sub baz\n";
	eval { bar(); };
	eval { foo(); };
}

eval { bar(); };
baz();
eval { foo(); };

