#!./perl -w

sub func_c {
    print "Function C\n";
    sleep 1;
}

sub func_b {
    print "Function B\n";
    sleep 1;
    func_c();
}

sub func_a {
    print "Function A\n";
    sleep 1;
    func_b();
}

func_a();
