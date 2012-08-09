#!./perl -w

sub func_c {
	print "Function C\n";
}

sub func_b {
	print "Function B\n";
	my $b = "B" x 100_000;
	func_c();
}

sub func_a {
	print "Function A\n";
	func_b();
}

func_a();
