package dumpvar;

# translate control chars to ^X - Randal Schwartz
sub unctrl {
	local($_) = @_;
	s/([\001-\037\177])/'^'.pack('c',ord($1)^64)/eg;
	$_;
}
sub main'dumpvar {
    ($package,@vars) = @_;
    local(*stab) = eval("*_$package");
    while (($key,$val) = each(%stab)) {
	{
	    next if @vars && !grep($key eq $_,@vars);
	    local(*entry) = $val;
	    if (defined $entry) {
		print "\$$key = '",&unctrl($entry),"'\n";
	    }
	    if (defined @entry) {
		print "\@$key = (\n";
		foreach $num ($[ .. $#entry) {
		    print "  $num\t'",&unctrl($entry[$num]),"'\n";
		}
		print ")\n";
	    }
	    if ($key ne "_$package" && $key ne "_DB" && defined %entry) {
		print "\%$key = (\n";
		foreach $key (sort keys(%entry)) {
		    print "  $key\t'",&unctrl($entry{$key}),"'\n";
		}
		print ")\n";
	    }
	}
    }
}

1;
