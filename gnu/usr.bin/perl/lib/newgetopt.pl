# newgetopt.pl -- new options parsing

# SCCS Status     : @(#)@ newgetopt.pl	1.13
# Author          : Johan Vromans
# Created On      : Tue Sep 11 15:00:12 1990
# Last Modified By: Johan Vromans
# Last Modified On: Tue Jun  2 11:24:03 1992
# Update Count    : 75
# Status          : Okay

# This package implements a new getopt function. This function adheres
# to the new syntax (long option names, no bundling).
#
# Arguments to the function are:
#
#  - a list of possible options. These should designate valid perl
#    identifiers, optionally followed by an argument specifier ("="
#    for mandatory arguments or ":" for optional arguments) and an
#    argument type specifier: "n" or "i" for integer numbers, "f" for
#    real (fix) numbers or "s" for strings.
#    If an "@" sign is appended, the option is treated as an array.
#    Value(s) are not set, but pushed.
#
#  - if the first option of the list consists of non-alphanumeric
#    characters only, it is interpreted as a generic option starter.
#    Everything starting with one of the characters from the starter
#    will be considered an option.
#    Likewise, a double occurrence (e.g. "--") signals end of
#    the options list.
#    The default value for the starter is "-", "--" or "+".
#
# Upon return, the option variables, prefixed with "opt_", are defined
# and set to the respective option arguments, if any.
# Options that do not take an argument are set to 1. Note that an
# option with an optional argument will be defined, but set to '' if
# no actual argument has been supplied.
# A return status of 0 (false) indicates that the function detected
# one or more errors.
#
# Special care is taken to give a correct treatment to optional arguments.
#
# E.g. if option "one:i" (i.e. takes an optional integer argument),
# then the following situations are handled:
#
#    -one -two		-> $opt_one = '', -two is next option
#    -one -2		-> $opt_one = -2
#
# Also, assume "foo=s" and "bar:s" :
#
#    -bar -xxx		-> $opt_bar = '', '-xxx' is next option
#    -foo -bar		-> $opt_foo = '-bar'
#    -foo --		-> $opt_foo = '--'
#
# HISTORY 
# 2-Jun-1992		Johan Vromans	
#    Do not use //o to allow multiple NGetOpt calls with different delimeters.
#    Prevent typeless option from using previous $array state.
#    Prevent empty option from being eaten as a (negative) number.

# 25-May-1992		Johan Vromans	
#    Add array options. "foo=s@" will return an array @opt_foo that
#    contains all values that were supplied. E.g. "-foo one -foo -two" will
#    return @opt_foo = ("one", "-two");
#    Correct bug in handling options that allow for a argument when followed
#    by another option.

# 4-May-1992		Johan Vromans	
#    Add $ignorecase to match options in either case.
#    Allow '' option.

# 19-Mar-1992		Johan Vromans	
#    Allow require from packages.
#    NGetOpt is now defined in the package that requires it.
#    @ARGV and $opt_... are taken from the package that calls it.
#    Use standard (?) option prefixes: -, -- and +.

# 20-Sep-1990		Johan Vromans	
#    Set options w/o argument to 1.
#    Correct the dreadful semicolon/require bug.


{   package newgetopt;
    $debug = 0;			# for debugging
    $ignorecase = 1;		# ignore case when matching options
}

sub NGetOpt {

    @newgetopt'optionlist = @_;
    *newgetopt'ARGV = *ARGV;

    package newgetopt;

    local ($[) = 0;
    local ($genprefix) = "(--|-|\\+)";
    local ($argend) = "--";
    local ($error) = 0;
    local ($opt, $optx, $arg, $type, $mand, %opctl);
    local ($pkg) = (caller)[0];

    print STDERR "NGetOpt 1.13 -- called from $pkg\n" if $debug;

    # See if the first element of the optionlist contains option
    # starter characters.
    if ( $optionlist[0] =~ /^\W+$/ ) {
	$genprefix = shift (@optionlist);
	# Turn into regexp.
	$genprefix =~ s/(\W)/\\\1/g;
	$genprefix = "[" . $genprefix . "]";
	undef $argend;
    }

    # Verify correctness of optionlist.
    %opctl = ();
    foreach $opt ( @optionlist ) {
	$opt =~ tr/A-Z/a-z/ if $ignorecase;
	if ( $opt !~ /^(\w*)([=:][infse]@?)?$/ ) {
	    print STDERR ("Error in option spec: \"", $opt, "\"\n");
	    $error++;
	    next;
	}
	$opctl{$1} = defined $2 ? $2 : "";
    }

    return 0 if $error;

    if ( $debug ) {
	local ($arrow, $k, $v);
	$arrow = "=> ";
	while ( ($k,$v) = each(%opctl) ) {
	    print STDERR ($arrow, "\$opctl{\"$k\"} = \"$v\"\n");
	    $arrow = "   ";
	}
    }

    # Process argument list

    while ( $#ARGV >= 0 ) {

	# >>> See also the continue block <<<

	# Get next argument
	$opt = shift (@ARGV);
	print STDERR ("=> option \"", $opt, "\"\n") if $debug;
	$arg = undef;

	# Check for exhausted list.
	if ( $opt =~ /^$genprefix/ ) {
	    # Double occurrence is terminator
	    return ($error == 0) 
		if ($opt eq "$+$+") || ((defined $argend) && $opt eq $argend);
	    $opt = $';		# option name (w/o prefix)
	}
	else {
	    # Apparently not an option - push back and exit.
	    unshift (@ARGV, $opt);
	    return ($error == 0);
	}

	# Look it up.
	$opt =~ tr/A-Z/a-z/ if $ignorecase;
	unless  ( defined ( $type = $opctl{$opt} ) ) {
	    print STDERR ("Unknown option: ", $opt, "\n");
	    $error++;
	    next;
	}

	# Determine argument status.
	print STDERR ("=> found \"$type\" for ", $opt, "\n") if $debug;

	# If it is an option w/o argument, we're almost finished with it.
	if ( $type eq "" ) {
	    $arg = 1;		# supply explicit value
	    $array = 0;
	    next;
	}

	# Get mandatory status and type info.
	($mand, $type, $array) = $type =~ /^(.)(.)(@?)$/;

	# Check if the argument list is exhausted.
	if ( $#ARGV < 0 ) {

	    # Complain if this option needs an argument.
	    if ( $mand eq "=" ) {
		print STDERR ("Option ", $opt, " requires an argument\n");
		$error++;
	    }
	    if ( $mand eq ":" ) {
		$arg = $type eq "s" ? "" : 0;
	    }
	    next;
	}

	# Get (possibly optional) argument.
	$arg = shift (@ARGV);

	# Check if it is a valid argument. A mandatory string takes
	# anything. 
	if ( "$mand$type" ne "=s" && $arg =~ /^$genprefix/ ) {

	    # Check for option list terminator.
	    if ( $arg eq "$+$+" || 
		 ((defined $argend) && $arg eq $argend)) {
		# Push back so the outer loop will terminate.
		unshift (@ARGV, $arg);
		# Complain if an argument is required.
		if ($mand eq "=") {
		    print STDERR ("Option ", $opt, " requires an argument\n");
		    $error++;
		    undef $arg;	# don't assign it
		}
		else {
		    # Supply empty value.
		    $arg = $type eq "s" ? "" : 0;
		}
		next;
	    }

	    # Maybe the optional argument is the next option?
	    if ( $mand eq ":" && ($' eq "" || $' =~ /[a-zA-Z_]/) ) {
		# Yep. Push back.
		unshift (@ARGV, $arg);
		$arg = $type eq "s" ? "" : 0;
		next;
	    }
	}

	if ( $type eq "n" || $type eq "i" ) { # numeric/integer
	    if ( $arg !~ /^-?[0-9]+$/ ) {
		print STDERR ("Value \"", $arg, "\" invalid for option ",
			      $opt, " (number expected)\n");
		$error++;
		undef $arg;	# don't assign it
	    }
	    next;
	}

	if ( $type eq "f" ) { # fixed real number, int is also ok
	    if ( $arg !~ /^-?[0-9.]+$/ ) {
		print STDERR ("Value \"", $arg, "\" invalid for option ",
			      $opt, " (real number expected)\n");
		$error++;
		undef $arg;	# don't assign it
	    }
	    next;
	}

	if ( $type eq "s" ) { # string
	    next;
	}

    }
    continue {
	if ( defined $arg ) {
	    if ( $array ) {
		print STDERR ('=> push (@', $pkg, '\'opt_', $opt, ", \"$arg\")\n")
		    if $debug;
	        eval ('push(@' . $pkg . '\'opt_' . $opt . ", \$arg);");
	    }
	    else {
		print STDERR ('=> $', $pkg, '\'opt_', $opt, " = \"$arg\"\n")
		    if $debug;
	        eval ('$' . $pkg . '\'opt_' . $opt . " = \$arg;");
	    }
	}
    }

    return ($error == 0);
}
1;
