#!./perl -w

sub func_c {
    print "Function C\n";
    for (my $i = 0; $i < 3000000; $i++) { my $j = $i + 1; }
}

sub func_b {
    print "Function B\n";
    for (my $i = 0; $i < 2000000; $i++) { my $j = $i + 1 ; }
    func_c();
}

sub func_a {
    print "Function A\n";
    for (my $i = 0; $i < 1000000; $i++) { my $j = $i + 1; }
    func_b();
}

func_a();
