package Getopt::Std;
require 5.000;
require Exporter;

=head1 NAME

getopt - Process single-character switches with switch clustering

getopts - Process single-character switches with switch clustering

=head1 SYNOPSIS

    use Getopt::Std;

    getopt('oDI');    # -o, -D & -I take arg.  Sets opt_* as a side effect.
    getopt('oDI', \%opts);    # -o, -D & -I take arg.  Values in %opts
    getopts('oif:');  # -o & -i are boolean flags, -f takes an argument
		      # Sets opt_* as a side effect.
    getopts('oif:', \%opts);  # options as above. Values in %opts

=head1 DESCRIPTION

The getopt() functions processes single-character switches with switch
clustering.  Pass one argument which is a string containing all switches
that take an argument.  For each switch found, sets $opt_x (where x is the
switch name) to the value of the argument, or 1 if no argument.  Switches
which take an argument don't care whether there is a space between the
switch and the argument.

Note that, if your code is running under the recommended C<use strict
'vars'> pragma, it may be helpful to declare these package variables
via C<use vars> perhaps something like this:

    use vars qw/ $opt_foo $opt_bar /;

For those of you who don't like additional variables being created, getopt()
and getopts() will also accept a hash reference as an optional second argument. 
Hash keys will be x (where x is the switch name) with key values the value of
the argument or 1 if no argument is specified.

=cut

@ISA = qw(Exporter);
@EXPORT = qw(getopt getopts);
$VERSION = $VERSION = '1.01';

# Process single-character switches with switch clustering.  Pass one argument
# which is a string containing all switches that take an argument.  For each
# switch found, sets $opt_x (where x is the switch name) to the value of the
# argument, or 1 if no argument.  Switches which take an argument don't care
# whether there is a space between the switch and the argument.

# Usage:
#	getopt('oDI');  # -o, -D & -I take arg.  Sets opt_* as a side effect.

sub getopt ($;$) {
    local($argumentative, $hash) = @_;
    local($_,$first,$rest);
    local @EXPORT;

    while (@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	if (index($argumentative,$first) >= 0) {
	    if ($rest ne '') {
		shift(@ARGV);
	    }
	    else {
		shift(@ARGV);
		$rest = shift(@ARGV);
	    }
          if (ref $hash) {
              $$hash{$first} = $rest;
          }
          else {
              ${"opt_$first"} = $rest;
              push( @EXPORT, "\$opt_$first" );
          }
	}
	else {
          if (ref $hash) {
              $$hash{$first} = 1;
          }
          else {
              ${"opt_$first"} = 1;
              push( @EXPORT, "\$opt_$first" );
          }
	    if ($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    unless (ref $hash) { 
	local $Exporter::ExportLevel = 1;
	import Getopt::Std;
    }
}

# Usage:
#   getopts('a:bc');	# -a takes arg. -b & -c not. Sets opt_* as a
#			#  side effect.

sub getopts ($;$) {
    local($argumentative, $hash) = @_;
    local(@args,$_,$first,$rest);
    local($errs) = 0;
    local @EXPORT;

    @args = split( / */, $argumentative );
    while(@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	$pos = index($argumentative,$first);
	if($pos >= 0) {
	    if(defined($args[$pos+1]) and ($args[$pos+1] eq ':')) {
		shift(@ARGV);
		if($rest eq '') {
		    ++$errs unless @ARGV;
		    $rest = shift(@ARGV);
		}
              if (ref $hash) {
                  $$hash{$first} = $rest;
              }
              else {
                  ${"opt_$first"} = $rest;
                  push( @EXPORT, "\$opt_$first" );
              }
	    }
	    else {
              if (ref $hash) {
                  $$hash{$first} = 1;
              }
              else {
                  ${"opt_$first"} = 1;
                  push( @EXPORT, "\$opt_$first" );
              }
		if($rest eq '') {
		    shift(@ARGV);
		}
		else {
		    $ARGV[0] = "-$rest";
		}
	    }
	}
	else {
	    warn "Unknown option: $first\n";
	    ++$errs;
	    if($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    unless (ref $hash) { 
	local $Exporter::ExportLevel = 1;
	import Getopt::Std;
    }
    $errs == 0;
}

1;

