# Test that dprof doesn't break
#    &bar;  used as &bar(@_);

sub foo1 {
	print "in foo1(@_)\n";
	bar(@_);
}
sub foo2 {
	print "in foo2(@_)\n";
	&bar;
}
sub bar {
	print "in bar(@_)\n";
	if( @_ > 0 ){
		&yeppers;
	}
}
sub yeppers {
	print "rest easy\n";
}


&foo1( A );
&foo2( B );

