sub foo {
	print "in sub foo\n";
	exit(0);
	bar();
}

sub bar {
	print "in sub bar\n";
}

sub baz {
	print "in sub baz\n";
	bar();
	foo();
}

bar();
baz();
foo();
