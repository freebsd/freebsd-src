package Carp;

=head1 NAME

Carp::Heavy - Carp guts

=head1 SYNOPIS

(internal use only)

=head1 DESCRIPTION

No user-serviceable parts inside.

=cut

# This package is heavily used. Be small. Be fast. Be good.

# Comments added by Andy Wardley <abw@kfs.org> 09-Apr-98, based on an
# _almost_ complete understanding of the package.  Corrections and
# comments are welcome.

# longmess() crawls all the way up the stack reporting on all the function
# calls made.  The error string, $error, is originally constructed from the
# arguments passed into longmess() via confess(), cluck() or shortmess().
# This gets appended with the stack trace messages which are generated for
# each function call on the stack.

sub longmess_heavy {
    return @_ if ref $_[0];
    my $error = join '', @_;
    my $mess = "";
    my $i = 1 + $CarpLevel;
    my ($pack,$file,$line,$sub,$hargs,$eval,$require);
    my (@a);
    #
    # crawl up the stack....
    #
    while (do { { package DB; @a = caller($i++) } } ) {
	# get copies of the variables returned from caller()
	($pack,$file,$line,$sub,$hargs,undef,$eval,$require) = @a;
	#
	# if the $error error string is newline terminated then it
	# is copied into $mess.  Otherwise, $mess gets set (at the end of
	# the 'else' section below) to one of two things.  The first time
	# through, it is set to the "$error at $file line $line" message.
	# $error is then set to 'called' which triggers subsequent loop
	# iterations to append $sub to $mess before appending the "$error
	# at $file line $line" which now actually reads "called at $file line
	# $line".  Thus, the stack trace message is constructed:
	#
	#        first time: $mess  = $error at $file line $line
	#  subsequent times: $mess .= $sub $error at $file line $line
	#                                  ^^^^^^
	#                                 "called"
	if ($error =~ m/\n$/) {
	    $mess .= $error;
	} else {
	    # Build a string, $sub, which names the sub-routine called.
	    # This may also be "require ...", "eval '...' or "eval {...}"
	    if (defined $eval) {
		if ($require) {
		    $sub = "require $eval";
		} else {
		    $eval =~ s/([\\\'])/\\$1/g;
		    if ($MaxEvalLen && length($eval) > $MaxEvalLen) {
			substr($eval,$MaxEvalLen) = '...';
		    }
		    $sub = "eval '$eval'";
		}
	    } elsif ($sub eq '(eval)') {
		$sub = 'eval {...}';
	    }
	    # if there are any arguments in the sub-routine call, format
	    # them according to the format variables defined earlier in
	    # this file and join them onto the $sub sub-routine string
	    if ($hargs) {
		# we may trash some of the args so we take a copy
		@a = @DB::args;	# must get local copy of args
		# don't print any more than $MaxArgNums
		if ($MaxArgNums and @a > $MaxArgNums) {
		    # cap the length of $#a and set the last element to '...'
		    $#a = $MaxArgNums;
		    $a[$#a] = "...";
		}
		for (@a) {
		    # set args to the string "undef" if undefined
		    $_ = "undef", next unless defined $_;
		    if (ref $_) {
			# force reference to string representation
			$_ .= '';
			s/'/\\'/g;
		    }
		    else {
			s/'/\\'/g;
			# terminate the string early with '...' if too long
			substr($_,$MaxArgLen) = '...'
			    if $MaxArgLen and $MaxArgLen < length;
		    }
		    # 'quote' arg unless it looks like a number
		    $_ = "'$_'" unless /^-?[\d.]+$/;
		    # print high-end chars as 'M-<char>'
		    s/([\200-\377])/sprintf("M-%c",ord($1)&0177)/eg;
		    # print remaining control chars as ^<char>
		    s/([\0-\37\177])/sprintf("^%c",ord($1)^64)/eg;
		}
		# append ('all', 'the', 'arguments') to the $sub string
		$sub .= '(' . join(', ', @a) . ')';
	    }
	    # here's where the error message, $mess, gets constructed
	    $mess .= "\t$sub " if $error eq "called";
	    $mess .= "$error at $file line $line";
	    if (defined &Thread::tid) {
		my $tid = Thread->self->tid;
		$mess .= " thread $tid" if $tid;
	    }
	    $mess .= "\n";
	}
	# we don't need to print the actual error message again so we can
	# change this to "called" so that the string "$error at $file line
	# $line" makes sense as "called at $file line $line".
	$error = "called";
    }
    $mess || $error;
}


# ancestors() returns the complete set of ancestors of a module

sub ancestors($$);

sub ancestors($$){
    my( $pack, $href ) = @_;
    if( @{"${pack}::ISA"} ){
	my $risa = \@{"${pack}::ISA"};
	my %tree  = ();
	@tree{@$risa} = ();
	foreach my $mod ( @$risa ){
	    # visit ancestors - if not already in the gallery
	    if( ! defined( $$href{$mod} ) ){
		my @ancs = ancestors( $mod, $href );
		@tree{@ancs} = ();
	    }
	}
	return ( keys( %tree ) );
    } else {
	return ();
    }
}


# shortmess() is called by carp() and croak() to skip all the way up to
# the top-level caller's package and report the error from there.  confess()
# and cluck() generate a full stack trace so they call longmess() to
# generate that.  In verbose mode shortmess() calls longmess() so
# you always get a stack trace

sub shortmess_heavy {	# Short-circuit &longmess if called via multiple packages
    goto &longmess_heavy if $Verbose;
    return @_ if ref $_[0];
    my $error = join '', @_;
    my ($prevpack) = caller(1);
    my $extra = $CarpLevel;

    my @Clans = ( $prevpack );
    my $i = 2;
    my ($pack,$file,$line);
    # when reporting an error, we want to report it from the context of the
    # calling package.  So what is the calling package?  Within a module,
    # there may be many calls between methods and perhaps between sub-classes
    # and super-classes, but the user isn't interested in what happens
    # inside the package.  We start by building a hash array which keeps
    # track of all the packages to which the calling package belongs.  We
    # do this by examining its @ISA variable.  Any call from a base class
    # method (one of our caller's @ISA packages) can be ignored
    my %isa;

    # merge all the caller's @ISA packages and ancestors into %isa.
    my @pars = ancestors( $prevpack, \%isa );
    @isa{@pars} = () if @pars;
    $isa{$prevpack} = 1;

    # now we crawl up the calling stack and look at all the packages in
    # there.  For each package, we look to see if it has an @ISA and then
    # we see if our caller features in that list.  That would imply that
    # our caller is a derived class of that package and its calls can also
    # be ignored
CALLER:
    while (($pack,$file,$line) = caller($i++)) {

        # Chances are, the caller's caller (or its caller...) is already
        # in the gallery - if so, ignore this caller.
        next if exists( $isa{$pack} );

        # no: collect this module's ancestors.
        my @i = ancestors( $pack, \%isa );
        my %i;
        if( @i ){
 	    @i{@i} = ();
            # check whether our representative of one of the clans is
            # in this family tree.
	    foreach my $cl (@Clans){
                if( exists( $i{$cl} ) ){
    	            # yes: merge all of the family tree into %isa
	            @isa{@i,$pack} = ();
		    # and here's where we do some more ignoring...
		    # if the package in question is one of our caller's
		    # base or derived packages then we can ignore it (skip it)
		    # and go onto the next.
		    next CALLER if exists( $isa{$pack} );
		    last;
		}
            }
	}

	# Hey!  We've found a package that isn't one of our caller's
	# clan....but wait, $extra refers to the number of 'extra' levels
	# we should skip up.  If $extra > 0 then this is a false alarm.
	# We must merge the package into the %isa hash (so we can ignore it
	# if it pops up again), decrement $extra, and continue.
	if ($extra-- > 0) {
	    push( @Clans, $pack );
	    @isa{@i,$pack} = ();
	}
	else {
	    # OK!  We've got a candidate package.  Time to construct the
	    # relevant error message and return it.
	    my $msg;
	    $msg = "$error at $file line $line";
	    if (defined &Thread::tid) {
		my $tid = Thread->self->tid;
		$msg .= " thread $tid" if $tid;
	    }
	    $msg .= "\n";
	    return $msg;
	}
    }

    # uh-oh!  It looks like we crawled all the way up the stack and
    # never found a candidate package.  Oh well, let's call longmess
    # to generate a full stack trace.  We use the magical form of 'goto'
    # so that this shortmess() function doesn't appear on the stack
    # to further confuse longmess() about it's calling package.
    goto &longmess_heavy;
}

1;
