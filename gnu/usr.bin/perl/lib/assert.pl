# assert.pl
# tchrist@convex.com (Tom Christiansen)
# 
# Usage:
# 
#     &assert('@x > @y');
#     &assert('$var > 10', $var, $othervar, @various_info);
# 
# That is, if the first expression evals false, we blow up.  The
# rest of the args, if any, are nice to know because they will
# be printed out by &panic, which is just the stack-backtrace
# routine shamelessly borrowed from the perl debugger.

sub assert {
    &panic("ASSERTION BOTCHED: $_[0]",$@) unless eval $_[0];
} 

sub panic {
    select(STDERR);

    print "\npanic: @_\n";

    exit 1 if $] <= 4.003;  # caller broken

    # stack traceback gratefully borrowed from perl debugger

    local($i,$_);
    local($p,$f,$l,$s,$h,$a,@a,@sub);
    for ($i = 0; ($p,$f,$l,$s,$h,$w) = caller($i); $i++) {
	@a = @DB'args;
	for (@a) {
	    if (/^StB\000/ && length($_) == length($_main{'_main'})) {
		$_ = sprintf("%s",$_);
	    }
	    else {
		s/'/\\'/g;
		s/([^\0]*)/'$1'/ unless /^-?[\d.]+$/;
		s/([\200-\377])/sprintf("M-%c",ord($1)&0177)/eg;
		s/([\0-\37\177])/sprintf("^%c",ord($1)^64)/eg;
	    }
	}
	$w = $w ? '@ = ' : '$ = ';
	$a = $h ? '(' . join(', ', @a) . ')' : '';
	push(@sub, "$w&$s$a from file $f line $l\n");
    }
    for ($i=0; $i <= $#sub; $i++) {
	print $sub[$i];
    }
    exit 1;
} 

1;
