# GetOpt::Long.pm -- Universal options parsing

package Getopt::Long;

# RCS Status      : $Id: GetoptLong.pl,v 2.24 2000-03-14 21:28:52+01 jv Exp $
# Author          : Johan Vromans
# Created On      : Tue Sep 11 15:00:12 1990
# Last Modified By: Johan Vromans
# Last Modified On: Tue Mar 14 21:28:40 2000
# Update Count    : 721
# Status          : Released

################ Copyright ################

# This program is Copyright 1990,2000 by Johan Vromans.
# This program is free software; you can redistribute it and/or
# modify it under the terms of the Perl Artistic License or the
# GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# If you do not have a copy of the GNU General Public License write to
# the Free Software Foundation, Inc., 675 Mass Ave, Cambridge,
# MA 02139, USA.

################ Module Preamble ################

use strict;

BEGIN {
    require 5.004;
    use Exporter ();
    use vars     qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
    $VERSION     = "2.23";

    @ISA         = qw(Exporter);
    @EXPORT      = qw(&GetOptions $REQUIRE_ORDER $PERMUTE $RETURN_IN_ORDER);
    %EXPORT_TAGS = qw();
    @EXPORT_OK   = qw();
    use AutoLoader qw(AUTOLOAD);
}

# User visible variables.
use vars @EXPORT, @EXPORT_OK;
use vars qw($error $debug $major_version $minor_version);
# Deprecated visible variables.
use vars qw($autoabbrev $getopt_compat $ignorecase $bundling $order
	    $passthrough);
# Official invisible variables.
use vars qw($genprefix $caller);

# Public subroutines.
sub Configure (@);
sub config (@);			# deprecated name
sub GetOptions;

# Private subroutines.
sub ConfigDefaults ();
sub FindOption ($$$$$$$);
sub Croak (@);			# demand loading the real Croak

################ Local Variables ################

################ Resident subroutines ################

sub ConfigDefaults () {
    # Handle POSIX compliancy.
    if ( defined $ENV{"POSIXLY_CORRECT"} ) {
	$genprefix = "(--|-)";
	$autoabbrev = 0;		# no automatic abbrev of options
	$bundling = 0;			# no bundling of single letter switches
	$getopt_compat = 0;		# disallow '+' to start options
	$order = $REQUIRE_ORDER;
    }
    else {
	$genprefix = "(--|-|\\+)";
	$autoabbrev = 1;		# automatic abbrev of options
	$bundling = 0;			# bundling off by default
	$getopt_compat = 1;		# allow '+' to start options
	$order = $PERMUTE;
    }
    # Other configurable settings.
    $debug = 0;			# for debugging
    $error = 0;			# error tally
    $ignorecase = 1;		# ignore case when matching options
    $passthrough = 0;		# leave unrecognized options alone
}

################ Initialization ################

# Values for $order. See GNU getopt.c for details.
($REQUIRE_ORDER, $PERMUTE, $RETURN_IN_ORDER) = (0..2);
# Version major/minor numbers.
($major_version, $minor_version) = $VERSION =~ /^(\d+)\.(\d+)/;

ConfigDefaults();

################ Package return ################

1;

__END__

################ AutoLoading subroutines ################

# RCS Status      : $Id: GetoptLongAl.pl,v 2.27 2000-03-17 09:07:26+01 jv Exp $
# Author          : Johan Vromans
# Created On      : Fri Mar 27 11:50:30 1998
# Last Modified By: Johan Vromans
# Last Modified On: Fri Mar 17 09:00:09 2000
# Update Count    : 55
# Status          : Released

sub GetOptions {

    my @optionlist = @_;	# local copy of the option descriptions
    my $argend = '--';		# option list terminator
    my %opctl = ();		# table of arg.specs (long and abbrevs)
    my %bopctl = ();		# table of arg.specs (bundles)
    my $pkg = $caller || (caller)[0];	# current context
				# Needed if linkage is omitted.
    my %aliases= ();		# alias table
    my @ret = ();		# accum for non-options
    my %linkage;		# linkage
    my $userlinkage;		# user supplied HASH
    my $opt;			# current option
    my $genprefix = $genprefix;	# so we can call the same module many times
    my @opctl;			# the possible long option names

    $error = '';

    print STDERR ("GetOpt::Long $Getopt::Long::VERSION ",
		  "called from package \"$pkg\".",
		  "\n  ",
		  'GetOptionsAl $Revision: 2.27 $ ',
		  "\n  ",
		  "ARGV: (@ARGV)",
		  "\n  ",
		  "autoabbrev=$autoabbrev,".
		  "bundling=$bundling,",
		  "getopt_compat=$getopt_compat,",
		  "order=$order,",
		  "\n  ",
		  "ignorecase=$ignorecase,",
		  "passthrough=$passthrough,",
		  "genprefix=\"$genprefix\".",
		  "\n")
	if $debug;

    # Check for ref HASH as first argument.
    # First argument may be an object. It's OK to use this as long
    # as it is really a hash underneath.
    $userlinkage = undef;
    if ( ref($optionlist[0]) and
	 "$optionlist[0]" =~ /^(?:.*\=)?HASH\([^\(]*\)$/ ) {
	$userlinkage = shift (@optionlist);
	print STDERR ("=> user linkage: $userlinkage\n") if $debug;
    }

    # See if the first element of the optionlist contains option
    # starter characters.
    # Be careful not to interpret '<>' as option starters.
    if ( $optionlist[0] =~ /^\W+$/
	 && !($optionlist[0] eq '<>'
	      && @optionlist > 0
	      && ref($optionlist[1])) ) {
	$genprefix = shift (@optionlist);
	# Turn into regexp. Needs to be parenthesized!
	$genprefix =~ s/(\W)/\\$1/g;
	$genprefix = "([" . $genprefix . "])";
    }

    # Verify correctness of optionlist.
    %opctl = ();
    %bopctl = ();
    while ( @optionlist > 0 ) {
	my $opt = shift (@optionlist);

	# Strip leading prefix so people can specify "--foo=i" if they like.
	$opt = $+ if $opt =~ /^$genprefix+(.*)$/s;

	if ( $opt eq '<>' ) {
	    if ( (defined $userlinkage)
		&& !(@optionlist > 0 && ref($optionlist[0]))
		&& (exists $userlinkage->{$opt})
		&& ref($userlinkage->{$opt}) ) {
		unshift (@optionlist, $userlinkage->{$opt});
	    }
	    unless ( @optionlist > 0
		    && ref($optionlist[0]) && ref($optionlist[0]) eq 'CODE' ) {
		$error .= "Option spec <> requires a reference to a subroutine\n";
		next;
	    }
	    $linkage{'<>'} = shift (@optionlist);
	    next;
	}

	# Match option spec. Allow '?' as an alias.
	if ( $opt !~ /^((\w+[-\w]*)(\|(\?|\w[-\w]*)?)*)?([!~+]|[=:][infse][@%]?)?$/ ) {
	    $error .= "Error in option spec: \"$opt\"\n";
	    next;
	}
	my ($o, $c, $a) = ($1, $5);
	$c = '' unless defined $c;

	if ( ! defined $o ) {
	    # empty -> '-' option
	    $opctl{$o = ''} = $c;
	}
	else {
	    # Handle alias names
	    my @o =  split (/\|/, $o);
	    my $linko = $o = $o[0];
	    # Force an alias if the option name is not locase.
	    $a = $o unless $o eq lc($o);
	    $o = lc ($o)
		if $ignorecase > 1
		    || ($ignorecase
			&& ($bundling ? length($o) > 1  : 1));

	    foreach ( @o ) {
		if ( $bundling && length($_) == 1 ) {
		    $_ = lc ($_) if $ignorecase > 1;
		    if ( $c eq '!' ) {
			$opctl{"no$_"} = $c;
			warn ("Ignoring '!' modifier for short option $_\n");
			$opctl{$_} = $bopctl{$_} = '';
		    }
		    else {
			$opctl{$_} = $bopctl{$_} = $c;
		    }
		}
		else {
		    $_ = lc ($_) if $ignorecase;
		    if ( $c eq '!' ) {
			$opctl{"no$_"} = $c;
			$opctl{$_} = ''
		    }
		    else {
			$opctl{$_} = $c;
		    }
		}
		if ( defined $a ) {
		    # Note alias.
		    $aliases{$_} = $a;
		}
		else {
		    # Set primary name.
		    $a = $_;
		}
	    }
	    $o = $linko;
	}

	# If no linkage is supplied in the @optionlist, copy it from
	# the userlinkage if available.
	if ( defined $userlinkage ) {
	    unless ( @optionlist > 0 && ref($optionlist[0]) ) {
		if ( exists $userlinkage->{$o} && ref($userlinkage->{$o}) ) {
		    print STDERR ("=> found userlinkage for \"$o\": ",
				  "$userlinkage->{$o}\n")
			if $debug;
		    unshift (@optionlist, $userlinkage->{$o});
		}
		else {
		    # Do nothing. Being undefined will be handled later.
		    next;
		}
	    }
	}

	# Copy the linkage. If omitted, link to global variable.
	if ( @optionlist > 0 && ref($optionlist[0]) ) {
	    print STDERR ("=> link \"$o\" to $optionlist[0]\n")
		if $debug;
	    if ( ref($optionlist[0]) =~ /^(SCALAR|CODE)$/ ) {
		$linkage{$o} = shift (@optionlist);
	    }
	    elsif ( ref($optionlist[0]) =~ /^(ARRAY)$/ ) {
		$linkage{$o} = shift (@optionlist);
		$opctl{$o} .= '@'
		  if $opctl{$o} ne '' and $opctl{$o} !~ /\@$/;
		$bopctl{$o} .= '@'
		  if $bundling and defined $bopctl{$o} and
		    $bopctl{$o} ne '' and $bopctl{$o} !~ /\@$/;
	    }
	    elsif ( ref($optionlist[0]) =~ /^(HASH)$/ ) {
		$linkage{$o} = shift (@optionlist);
		$opctl{$o} .= '%'
		  if $opctl{$o} ne '' and $opctl{$o} !~ /\%$/;
		$bopctl{$o} .= '%'
		  if $bundling and defined $bopctl{$o} and
		    $bopctl{$o} ne '' and $bopctl{$o} !~ /\%$/;
	    }
	    else {
		$error .= "Invalid option linkage for \"$opt\"\n";
	    }
	}
	else {
	    # Link to global $opt_XXX variable.
	    # Make sure a valid perl identifier results.
	    my $ov = $o;
	    $ov =~ s/\W/_/g;
	    if ( $c =~ /@/ ) {
		print STDERR ("=> link \"$o\" to \@$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$o} = \\\@".$pkg."::opt_$ov;");
	    }
	    elsif ( $c =~ /%/ ) {
		print STDERR ("=> link \"$o\" to \%$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$o} = \\\%".$pkg."::opt_$ov;");
	    }
	    else {
		print STDERR ("=> link \"$o\" to \$$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$o} = \\\$".$pkg."::opt_$ov;");
	    }
	}
    }

    # Bail out if errors found.
    die ($error) if $error;
    $error = 0;

    # Sort the possible long option names.
    @opctl = sort(keys (%opctl)) if $autoabbrev;

    # Show the options tables if debugging.
    if ( $debug ) {
	my ($arrow, $k, $v);
	$arrow = "=> ";
	while ( ($k,$v) = each(%opctl) ) {
	    print STDERR ($arrow, "\$opctl{\"$k\"} = \"$v\"\n");
	    $arrow = "   ";
	}
	$arrow = "=> ";
	while ( ($k,$v) = each(%bopctl) ) {
	    print STDERR ($arrow, "\$bopctl{\"$k\"} = \"$v\"\n");
	    $arrow = "   ";
	}
    }

    # Process argument list
    my $goon = 1;
    while ( $goon && @ARGV > 0 ) {

	#### Get next argument ####

	$opt = shift (@ARGV);
	print STDERR ("=> option \"", $opt, "\"\n") if $debug;

	#### Determine what we have ####

	# Double dash is option list terminator.
	if ( $opt eq $argend ) {
	    # Finish. Push back accumulated arguments and return.
	    unshift (@ARGV, @ret)
		if $order == $PERMUTE;
	    return ($error == 0);
	}

	my $tryopt = $opt;
	my $found;		# success status
	my $dsttype;		# destination type ('@' or '%')
	my $incr;		# destination increment
	my $key;		# key (if hash type)
	my $arg;		# option argument

	($found, $opt, $arg, $dsttype, $incr, $key) =
	  FindOption ($genprefix, $argend, $opt,
		      \%opctl, \%bopctl, \@opctl, \%aliases);

	if ( $found ) {

	    # FindOption undefines $opt in case of errors.
	    next unless defined $opt;

	    if ( defined $arg ) {
		$opt = $aliases{$opt} if defined $aliases{$opt};

		if ( defined $linkage{$opt} ) {
		    print STDERR ("=> ref(\$L{$opt}) -> ",
				  ref($linkage{$opt}), "\n") if $debug;

		    if ( ref($linkage{$opt}) eq 'SCALAR' ) {
			if ( $incr ) {
			    print STDERR ("=> \$\$L{$opt} += \"$arg\"\n")
			      if $debug;
			    if ( defined ${$linkage{$opt}} ) {
			        ${$linkage{$opt}} += $arg;
			    }
		            else {
			        ${$linkage{$opt}} = $arg;
			    }
			}
			else {
			    print STDERR ("=> \$\$L{$opt} = \"$arg\"\n")
			      if $debug;
			    ${$linkage{$opt}} = $arg;
		        }
		    }
		    elsif ( ref($linkage{$opt}) eq 'ARRAY' ) {
			print STDERR ("=> push(\@{\$L{$opt}, \"$arg\")\n")
			    if $debug;
			push (@{$linkage{$opt}}, $arg);
		    }
		    elsif ( ref($linkage{$opt}) eq 'HASH' ) {
			print STDERR ("=> \$\$L{$opt}->{$key} = \"$arg\"\n")
			    if $debug;
			$linkage{$opt}->{$key} = $arg;
		    }
		    elsif ( ref($linkage{$opt}) eq 'CODE' ) {
			print STDERR ("=> &L{$opt}(\"$opt\", \"$arg\")\n")
			    if $debug;
			local ($@);
			eval {
			    &{$linkage{$opt}}($opt, $arg);
			};
			print STDERR ("=> die($@)\n") if $debug && $@ ne '';
			if ( $@ =~ /^!/ ) {
			    if ( $@ =~ /^!FINISH\b/ ) {
				$goon = 0;
			    }
			}
			elsif ( $@ ne '' ) {
			    warn ($@);
			    $error++;
			}
		    }
		    else {
			print STDERR ("Invalid REF type \"", ref($linkage{$opt}),
				      "\" in linkage\n");
			Croak ("Getopt::Long -- internal error!\n");
		    }
		}
		# No entry in linkage means entry in userlinkage.
		elsif ( $dsttype eq '@' ) {
		    if ( defined $userlinkage->{$opt} ) {
			print STDERR ("=> push(\@{\$L{$opt}}, \"$arg\")\n")
			    if $debug;
			push (@{$userlinkage->{$opt}}, $arg);
		    }
		    else {
			print STDERR ("=>\$L{$opt} = [\"$arg\"]\n")
			    if $debug;
			$userlinkage->{$opt} = [$arg];
		    }
		}
		elsif ( $dsttype eq '%' ) {
		    if ( defined $userlinkage->{$opt} ) {
			print STDERR ("=> \$L{$opt}->{$key} = \"$arg\"\n")
			    if $debug;
			$userlinkage->{$opt}->{$key} = $arg;
		    }
		    else {
			print STDERR ("=>\$L{$opt} = {$key => \"$arg\"}\n")
			    if $debug;
			$userlinkage->{$opt} = {$key => $arg};
		    }
		}
		else {
		    if ( $incr ) {
			print STDERR ("=> \$L{$opt} += \"$arg\"\n")
			  if $debug;
			if ( defined $userlinkage->{$opt} ) {
			    $userlinkage->{$opt} += $arg;
			}
			else {
			    $userlinkage->{$opt} = $arg;
			}
		    }
		    else {
			print STDERR ("=>\$L{$opt} = \"$arg\"\n") if $debug;
			$userlinkage->{$opt} = $arg;
		    }
		}
	    }
	}

	# Not an option. Save it if we $PERMUTE and don't have a <>.
	elsif ( $order == $PERMUTE ) {
	    # Try non-options call-back.
	    my $cb;
	    if ( (defined ($cb = $linkage{'<>'})) ) {
		local ($@);
		eval {
		    &$cb ($tryopt);
		};
		print STDERR ("=> die($@)\n") if $debug && $@ ne '';
		if ( $@ =~ /^!/ ) {
		    if ( $@ =~ /^!FINISH\b/ ) {
			$goon = 0;
		    }
		}
		elsif ( $@ ne '' ) {
		    warn ($@);
		    $error++;
		}
	    }
	    else {
		print STDERR ("=> saving \"$tryopt\" ",
			      "(not an option, may permute)\n") if $debug;
		push (@ret, $tryopt);
	    }
	    next;
	}

	# ...otherwise, terminate.
	else {
	    # Push this one back and exit.
	    unshift (@ARGV, $tryopt);
	    return ($error == 0);
	}

    }

    # Finish.
    if ( $order == $PERMUTE ) {
	#  Push back accumulated arguments
	print STDERR ("=> restoring \"", join('" "', @ret), "\"\n")
	    if $debug && @ret > 0;
	unshift (@ARGV, @ret) if @ret > 0;
    }

    return ($error == 0);
}

# Option lookup.
sub FindOption ($$$$$$$) {

    # returns (1, $opt, $arg, $dsttype, $incr, $key) if okay,
    # returns (0) otherwise.

    my ($prefix, $argend, $opt, $opctl, $bopctl, $names, $aliases) = @_;
    my $key;			# hash key for a hash option
    my $arg;

    print STDERR ("=> find \"$opt\", prefix=\"$prefix\"\n") if $debug;

    return (0) unless $opt =~ /^$prefix(.*)$/s;

    $opt = $+;
    my ($starter) = $1;

    print STDERR ("=> split \"$starter\"+\"$opt\"\n") if $debug;

    my $optarg = undef;	# value supplied with --opt=value
    my $rest = undef;	# remainder from unbundling

    # If it is a long option, it may include the value.
    if (($starter eq "--" || ($getopt_compat && !$bundling))
	&& $opt =~ /^([^=]+)=(.*)$/s ) {
	$opt = $1;
	$optarg = $2;
	print STDERR ("=> option \"", $opt,
		      "\", optarg = \"$optarg\"\n") if $debug;
    }

    #### Look it up ###

    my $tryopt = $opt;		# option to try
    my $optbl = $opctl;		# table to look it up (long names)
    my $type;
    my $dsttype = '';
    my $incr = 0;

    if ( $bundling && $starter eq '-' ) {
	# Unbundle single letter option.
	$rest = substr ($tryopt, 1);
	$tryopt = substr ($tryopt, 0, 1);
	$tryopt = lc ($tryopt) if $ignorecase > 1;
	print STDERR ("=> $starter$tryopt unbundled from ",
		      "$starter$tryopt$rest\n") if $debug;
	$rest = undef unless $rest ne '';
	$optbl = $bopctl;	# look it up in the short names table

	# If bundling == 2, long options can override bundles.
	if ( $bundling == 2 and
	     defined ($rest) and
	     defined ($type = $opctl->{$tryopt.$rest}) ) {
	    print STDERR ("=> $starter$tryopt rebundled to ",
			  "$starter$tryopt$rest\n") if $debug;
	    $tryopt .= $rest;
	    undef $rest;
	}
    }

    # Try auto-abbreviation.
    elsif ( $autoabbrev ) {
	# Downcase if allowed.
	$tryopt = $opt = lc ($opt) if $ignorecase;
	# Turn option name into pattern.
	my $pat = quotemeta ($opt);
	# Look up in option names.
	my @hits = grep (/^$pat/, @{$names});
	print STDERR ("=> ", scalar(@hits), " hits (@hits) with \"$pat\" ",
		      "out of ", scalar(@{$names}), "\n") if $debug;

	# Check for ambiguous results.
	unless ( (@hits <= 1) || (grep ($_ eq $opt, @hits) == 1) ) {
	    # See if all matches are for the same option.
	    my %hit;
	    foreach ( @hits ) {
		$_ = $aliases->{$_} if defined $aliases->{$_};
		$hit{$_} = 1;
	    }
	    # Now see if it really is ambiguous.
	    unless ( keys(%hit) == 1 ) {
		return (0) if $passthrough;
		warn ("Option ", $opt, " is ambiguous (",
		      join(", ", @hits), ")\n");
		$error++;
		undef $opt;
		return (1, $opt,$arg,$dsttype,$incr,$key);
	    }
	    @hits = keys(%hit);
	}

	# Complete the option name, if appropriate.
	if ( @hits == 1 && $hits[0] ne $opt ) {
	    $tryopt = $hits[0];
	    $tryopt = lc ($tryopt) if $ignorecase;
	    print STDERR ("=> option \"$opt\" -> \"$tryopt\"\n")
		if $debug;
	}
    }

    # Map to all lowercase if ignoring case.
    elsif ( $ignorecase ) {
	$tryopt = lc ($opt);
    }

    # Check validity by fetching the info.
    $type = $optbl->{$tryopt} unless defined $type;
    unless  ( defined $type ) {
	return (0) if $passthrough;
	warn ("Unknown option: ", $opt, "\n");
	$error++;
	return (1, $opt,$arg,$dsttype,$incr,$key);
    }
    # Apparently valid.
    $opt = $tryopt;
    print STDERR ("=> found \"$type\" for ", $opt, "\n") if $debug;

    #### Determine argument status ####

    # If it is an option w/o argument, we're almost finished with it.
    if ( $type eq '' || $type eq '!' || $type eq '+' ) {
	if ( defined $optarg ) {
	    return (0) if $passthrough;
	    warn ("Option ", $opt, " does not take an argument\n");
	    $error++;
	    undef $opt;
	}
	elsif ( $type eq '' || $type eq '+' ) {
	    $arg = 1;		# supply explicit value
	    $incr = $type eq '+';
	}
	else {
	    substr ($opt, 0, 2) = ''; # strip NO prefix
	    $arg = 0;		# supply explicit value
	}
	unshift (@ARGV, $starter.$rest) if defined $rest;
	return (1, $opt,$arg,$dsttype,$incr,$key);
    }

    # Get mandatory status and type info.
    my $mand;
    ($mand, $type, $dsttype, $key) = $type =~ /^(.)(.)([@%]?)$/;

    # Check if there is an option argument available.
    if ( defined $optarg ? ($optarg eq '')
	 : !(defined $rest || @ARGV > 0) ) {
	# Complain if this option needs an argument.
	if ( $mand eq "=" ) {
	    return (0) if $passthrough;
	    warn ("Option ", $opt, " requires an argument\n");
	    $error++;
	    undef $opt;
	}
	if ( $mand eq ":" ) {
	    $arg = $type eq "s" ? '' : 0;
	}
	return (1, $opt,$arg,$dsttype,$incr,$key);
    }

    # Get (possibly optional) argument.
    $arg = (defined $rest ? $rest
	    : (defined $optarg ? $optarg : shift (@ARGV)));

    # Get key if this is a "name=value" pair for a hash option.
    $key = undef;
    if ($dsttype eq '%' && defined $arg) {
	($key, $arg) = ($arg =~ /^([^=]*)=(.*)$/s) ? ($1, $2) : ($arg, 1);
    }

    #### Check if the argument is valid for this option ####

    if ( $type eq "s" ) {	# string
	# A mandatory string takes anything.
	return (1, $opt,$arg,$dsttype,$incr,$key) if $mand eq "=";

	# An optional string takes almost anything.
	return (1, $opt,$arg,$dsttype,$incr,$key)
	  if defined $optarg || defined $rest;
	return (1, $opt,$arg,$dsttype,$incr,$key) if $arg eq "-"; # ??

	# Check for option or option list terminator.
	if ($arg eq $argend ||
	    $arg =~ /^$prefix.+/) {
	    # Push back.
	    unshift (@ARGV, $arg);
	    # Supply empty value.
	    $arg = '';
	}
    }

    elsif ( $type eq "n" || $type eq "i" ) { # numeric/integer
	if ( $bundling && defined $rest && $rest =~ /^([-+]?[0-9]+)(.*)$/s ) {
	    $arg = $1;
	    $rest = $2;
	    unshift (@ARGV, $starter.$rest) if defined $rest && $rest ne '';
	}
	elsif ( $arg !~ /^[-+]?[0-9]+$/ ) {
	    if ( defined $optarg || $mand eq "=" ) {
		if ( $passthrough ) {
		    unshift (@ARGV, defined $rest ? $starter.$rest : $arg)
		      unless defined $optarg;
		    return (0);
		}
		warn ("Value \"", $arg, "\" invalid for option ",
		      $opt, " (number expected)\n");
		$error++;
		undef $opt;
		# Push back.
		unshift (@ARGV, $starter.$rest) if defined $rest;
	    }
	    else {
		# Push back.
		unshift (@ARGV, defined $rest ? $starter.$rest : $arg);
		# Supply default value.
		$arg = 0;
	    }
	}
    }

    elsif ( $type eq "f" ) { # real number, int is also ok
	# We require at least one digit before a point or 'e',
	# and at least one digit following the point and 'e'.
	# [-]NN[.NN][eNN]
	if ( $bundling && defined $rest &&
	     $rest =~ /^([-+]?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?)(.*)$/s ) {
	    $arg = $1;
	    $rest = $+;
	    unshift (@ARGV, $starter.$rest) if defined $rest && $rest ne '';
	}
	elsif ( $arg !~ /^[-+]?[0-9.]+(\.[0-9]+)?([eE][-+]?[0-9]+)?$/ ) {
	    if ( defined $optarg || $mand eq "=" ) {
		if ( $passthrough ) {
		    unshift (@ARGV, defined $rest ? $starter.$rest : $arg)
		      unless defined $optarg;
		    return (0);
		}
		warn ("Value \"", $arg, "\" invalid for option ",
		      $opt, " (real number expected)\n");
		$error++;
		undef $opt;
		# Push back.
		unshift (@ARGV, $starter.$rest) if defined $rest;
	    }
	    else {
		# Push back.
		unshift (@ARGV, defined $rest ? $starter.$rest : $arg);
		# Supply default value.
		$arg = 0.0;
	    }
	}
    }
    else {
	Croak ("GetOpt::Long internal error (Can't happen)\n");
    }
    return (1, $opt, $arg, $dsttype, $incr, $key);
}

# Getopt::Long Configuration.
sub Configure (@) {
    my (@options) = @_;

    my $prevconfig =
      [ $error, $debug, $major_version, $minor_version,
	$autoabbrev, $getopt_compat, $ignorecase, $bundling, $order,
	$passthrough, $genprefix ];

    if ( ref($options[0]) eq 'ARRAY' ) {
	( $error, $debug, $major_version, $minor_version,
	  $autoabbrev, $getopt_compat, $ignorecase, $bundling, $order,
	  $passthrough, $genprefix ) = @{shift(@options)};
    }

    my $opt;
    foreach $opt ( @options ) {
	my $try = lc ($opt);
	my $action = 1;
	if ( $try =~ /^no_?(.*)$/s ) {
	    $action = 0;
	    $try = $+;
	}
	if ( $try eq 'default' or $try eq 'defaults' ) {
	    ConfigDefaults () if $action;
	}
	elsif ( $try eq 'auto_abbrev' or $try eq 'autoabbrev' ) {
	    $autoabbrev = $action;
	}
	elsif ( $try eq 'getopt_compat' ) {
	    $getopt_compat = $action;
	}
	elsif ( $try eq 'ignorecase' or $try eq 'ignore_case' ) {
	    $ignorecase = $action;
	}
	elsif ( $try eq 'ignore_case_always' ) {
	    $ignorecase = $action ? 2 : 0;
	}
	elsif ( $try eq 'bundling' ) {
	    $bundling = $action;
	}
	elsif ( $try eq 'bundling_override' ) {
	    $bundling = $action ? 2 : 0;
	}
	elsif ( $try eq 'require_order' ) {
	    $order = $action ? $REQUIRE_ORDER : $PERMUTE;
	}
	elsif ( $try eq 'permute' ) {
	    $order = $action ? $PERMUTE : $REQUIRE_ORDER;
	}
	elsif ( $try eq 'pass_through' or $try eq 'passthrough' ) {
	    $passthrough = $action;
	}
	elsif ( $try =~ /^prefix=(.+)$/ ) {
	    $genprefix = $1;
	    # Turn into regexp. Needs to be parenthesized!
	    $genprefix = "(" . quotemeta($genprefix) . ")";
	    eval { '' =~ /$genprefix/; };
	    Croak ("Getopt::Long: invalid pattern \"$genprefix\"") if $@;
	}
	elsif ( $try =~ /^prefix_pattern=(.+)$/ ) {
	    $genprefix = $1;
	    # Parenthesize if needed.
	    $genprefix = "(" . $genprefix . ")"
	      unless $genprefix =~ /^\(.*\)$/;
	    eval { '' =~ /$genprefix/; };
	    Croak ("Getopt::Long: invalid pattern \"$genprefix\"") if $@;
	}
	elsif ( $try eq 'debug' ) {
	    $debug = $action;
	}
	else {
	    Croak ("Getopt::Long: unknown config parameter \"$opt\"")
	}
    }
    $prevconfig;
}

# Deprecated name.
sub config (@) {
    Configure (@_);
}

# To prevent Carp from being loaded unnecessarily.
sub Croak (@) {
    require 'Carp.pm';
    $Carp::CarpLevel = 1;
    Carp::croak(@_);
};

################ Documentation ################

=head1 NAME

Getopt::Long - Extended processing of command line options

=head1 SYNOPSIS

  use Getopt::Long;
  $result = GetOptions (...option-descriptions...);

=head1 DESCRIPTION

The Getopt::Long module implements an extended getopt function called
GetOptions(). This function adheres to the POSIX syntax for command
line options, with GNU extensions. In general, this means that options
have long names instead of single letters, and are introduced with a
double dash "--". Support for bundling of command line options, as was
the case with the more traditional single-letter approach, is provided
but not enabled by default.

=head1 Command Line Options, an Introduction

Command line operated programs traditionally take their arguments from
the command line, for example filenames or other information that the
program needs to know. Besides arguments, these programs often take
command line I<options> as well. Options are not necessary for the
program to work, hence the name 'option', but are used to modify its
default behaviour. For example, a program could do its job quietly,
but with a suitable option it could provide verbose information about
what it did.

Command line options come in several flavours. Historically, they are
preceded by a single dash C<->, and consist of a single letter.

    -l -a -c

Usually, these single-character options can be bundled:

    -lac

Options can have values, the value is placed after the option
character. Sometimes with whitespace in between, sometimes not:

    -s 24 -s24

Due to the very cryptic nature of these options, another style was
developed that used long names. So instead of a cryptic C<-l> one
could use the more descriptive C<--long>. To distinguish between a
bundle of single-character options and a long one, two dashes are used
to precede the option name. Early implementations of long options used
a plus C<+> instead. Also, option values could be specified either
like 

    --size=24

or

    --size 24

The C<+> form is now obsolete and strongly deprecated.

=head1 Getting Started with Getopt::Long

Getopt::Long is the Perl5 successor of C<newgetopt.pl>. This was
the firs Perl module that provided support for handling the new style
of command line options, hence the name Getopt::Long. This module
also supports single-character options and bundling. In this case, the
options are restricted to alphabetic characters only, and the
characters C<?> and C<->.

To use Getopt::Long from a Perl program, you must include the
following line in your Perl program:

    use Getopt::Long;

This will load the core of the Getopt::Long module and prepare your
program for using it. Most of the actual Getopt::Long code is not
loaded until you really call one of its functions.

In the default configuration, options names may be abbreviated to
uniqueness, case does not matter, and a single dash is sufficient,
even for long option names. Also, options may be placed between
non-option arguments. See L<Configuring Getopt::Long> for more
details on how to configure Getopt::Long.

=head2 Simple options

The most simple options are the ones that take no values. Their mere
presence on the command line enables the option. Popular examples are:

    --all --verbose --quiet --debug

Handling simple options is straightforward:

    my $verbose = '';	# option variable with default value (false)
    my $all = '';	# option variable with default value (false)
    GetOptions ('verbose' => \$verbose, 'all' => \$all);

The call to GetOptions() parses the command line arguments that are
present in C<@ARGV> and sets the option variable to the value C<1> if
the option did occur on the command line. Otherwise, the option
variable is not touched. Setting the option value to true is often
called I<enabling> the option.

The option name as specified to the GetOptions() function is called
the option I<specification>. Later we'll see that this specification
can contain more than just the option name. The reference to the
variable is called the option I<destination>.

GetOptions() will return a true value if the command line could be
processed successfully. Otherwise, it will write error messages to
STDERR, and return a false result.

=head2 A little bit less simple options

Getopt::Long supports two useful variants of simple options:
I<negatable> options and I<incremental> options.

A negatable option is specified with a exclamation mark C<!> after the
option name:

    my $verbose = '';	# option variable with default value (false)
    GetOptions ('verbose!' => \$verbose);

Now, using C<--verbose> on the command line will enable C<$verbose>,
as expected. But it is also allowed to use C<--noverbose>, which will
disable C<$verbose> by setting its value to C<0>. Using a suitable
default value, the program can find out whether C<$verbose> is false
by default, or disabled by using C<--noverbose>.

An incremental option is specified with a plus C<+> after the
option name:

    my $verbose = '';	# option variable with default value (false)
    GetOptions ('verbose+' => \$verbose);

Using C<--verbose> on the command line will increment the value of
C<$verbose>. This way the program can keep track of how many times the
option occurred on the command line. For example, each occurrence of
C<--verbose> could increase the verbosity level of the program.

=head2 Mixing command line option with other arguments

Usually programs take command line options as well as other arguments,
for example, file names. It is good practice to always specify the
options first, and the other arguments last. Getopt::Long will,
however, allow the options and arguments to be mixed and 'filter out'
all the options before passing the rest of the arguments to the
program. To stop Getopt::Long from processing further arguments,
insert a double dash C<--> on the command line:

    --size 24 -- --all

In this example, C<--all> will I<not> be treated as an option, but
passed to the program unharmed, in C<@ARGV>.

=head2 Options with values

For options that take values it must be specified whether the option
value is required or not, and what kind of value the option expects.

Three kinds of values are supported: integer numbers, floating point
numbers, and strings.

If the option value is required, Getopt::Long will take the
command line argument that follows the option and assign this to the
option variable. If, however, the option value is specified as
optional, this will only be done if that value does not look like a
valid command line option itself.

    my $tag = '';	# option variable with default value
    GetOptions ('tag=s' => \$tag);

In the option specification, the option name is followed by an equals
sign C<=> and the letter C<s>. The equals sign indicates that this
option requires a value. The letter C<s> indicates that this value is
an arbitrary string. Other possible value types are C<i> for integer
values, and C<f> for floating point values. Using a colon C<:> instead
of the equals sign indicates that the option value is optional. In
this case, if no suitable value is supplied, string valued options get
an empty string C<''> assigned, while numeric options are set to C<0>.

=head2 Options with multiple values

Options sometimes take several values. For example, a program could
use multiple directories to search for library files:

    --library lib/stdlib --library lib/extlib

To accomplish this behaviour, simply specify an array reference as the
destination for the option:

    my @libfiles = ();
    GetOptions ("library=s" => \@libfiles);

Used with the example above, C<@libfiles> would contain two strings
upon completion: C<"lib/srdlib"> and C<"lib/extlib">, in that order.
It is also possible to specify that only integer or floating point
numbers are acceptible values.

Often it is useful to allow comma-separated lists of values as well as
multiple occurrences of the options. This is easy using Perl's split()
and join() operators:

    my @libfiles = ();
    GetOptions ("library=s" => \@libfiles);
    @libfiles = split(/,/,join(',',@libfiles));

Of course, it is important to choose the right separator string for
each purpose.

=head2 Options with hash values

If the option destination is a reference to a hash, the option will
take, as value, strings of the form I<key>C<=>I<value>. The value will
be stored with the specified key in the hash.

    my %defines = ();
    GetOptions ("define=s" => \%defines);

When used with command line options:

    --define os=linux --define vendor=redhat

the hash C<%defines> will contain two keys, C<"os"> with value
C<"linux> and C<"vendor"> with value C<"redhat">.
It is also possible to specify that only integer or floating point
numbers are acceptible values. The keys are always taken to be strings.

=head2 User-defined subroutines to handle options

Ultimate control over what should be done when (actually: each time)
an option is encountered on the command line can be achieved by
designating a reference to a subroutine (or an anonymous subroutine)
as the option destination. When GetOptions() encounters the option, it
will call the subroutine with two arguments: the name of the option,
and the value to be assigned. It is up to the subroutine to store the
value, or do whatever it thinks is appropriate.

A trivial application of this mechanism is to implement options that
are related to each other. For example:

    my $verbose = '';	# option variable with default value (false)
    GetOptions ('verbose' => \$verbose,
	        'quiet'   => sub { $verbose = 0 });

Here C<--verbose> and C<--quiet> control the same variable
C<$verbose>, but with opposite values.

If the subroutine needs to signal an error, it should call die() with
the desired error message as its argument. GetOptions() will catch the
die(), issue the error message, and record that an error result must
be returned upon completion.

If the text of the error message starts with an exclamantion mark C<!>
it is interpreted specially by GetOptions(). There is currently one
special command implemented: C<die("!FINISH")> will cause GetOptions()
to stop processing options, as if it encountered a double dash C<-->.

=head2 Options with multiple names

Often it is user friendly to supply alternate mnemonic names for
options. For example C<--height> could be an alternate name for
C<--length>. Alternate names can be included in the option
specification, separated by vertical bar C<|> characters. To implement
the above example:

    GetOptions ('length|height=f' => \$length);

The first name is called the I<primary> name, the other names are
called I<aliases>.

Multiple alternate names are possible.

=head2 Case and abbreviations

Without additional configuration, GetOptions() will ignore the case of
option names, and allow the options to be abbreviated to uniqueness.

    GetOptions ('length|height=f' => \$length, "head" => \$head);

This call will allow C<--l> and C<--L> for the length option, but
requires a least C<--hea> and C<--hei> for the head and height options.

=head2 Summary of Option Specifications

Each option specifier consists of two parts: the name specification
and the argument specification. 

The name specification contains the name of the option, optionally
followed by a list of alternative names separated by vertical bar
characters. 

    length	      option name is "length"
    length|size|l     name is "length", aliases are "size" and "l"

The argument specification is optional. If omitted, the option is
considered boolean, a value of 1 will be assigned when the option is
used on the command line.

The argument specification can be

=over

=item !

The option does not take an argument and may be negated, i.e. prefixed
by "no". E.g. C<"foo!"> will allow C<--foo> (a value of 1 will be
assigned) and C<--nofoo> (a value of 0 will be assigned). If the
option has aliases, this applies to the aliases as well.

Using negation on a single letter option when bundling is in effect is
pointless and will result in a warning.

=item +

The option does not take an argument and will be incremented by 1
every time it appears on the command line. E.g. C<"more+">, when used
with C<--more --more --more>, will increment the value three times,
resulting in a value of 3 (provided it was 0 or undefined at first).

The C<+> specifier is ignored if the option destination is not a scalar.

=item = I<type> [ I<desttype> ]

The option requires an argument of the given type. Supported types
are:

=over

=item s

String. An arbitrary sequence of characters. It is valid for the
argument to start with C<-> or C<-->.

=item i

Integer. An optional leading plus or minus sign, followed by a
sequence of digits.

=item f

Real number. For example C<3.14>, C<-6.23E24> and so on.

=back

The I<desttype> can be C<@> or C<%> to specify that the option is
list or a hash valued. This is only needed when the destination for
the option value is not otherwise specified. It should be omitted when
not needed.

=item : I<type> [ I<desttype> ]

Like C<=>, but designates the argument as optional.
If omitted, an empty string will be assigned to string values options,
and the value zero to numeric options.

Note that if a string argument starts with C<-> or C<-->, it will be
considered an option on itself.

=back

=head1 Advanced Possibilities

=head2 Documentation and help texts

Getopt::Long encourages the use of Pod::Usage to produce help
messages. For example:

    use Getopt::Long;
    use Pod::Usage;

    my $man = 0;
    my $help = 0;

    GetOptions('help|?' => \$help, man => \$man) or pod2usage(2);
    pod2usage(1) if $help;
    pod2usage(-exitstatus => 0, -verbose => 2) if $man;

    __END__

    =head1 NAME

    sample - Using GetOpt::Long and Pod::Usage

    =head1 SYNOPSIS

    sample [options] [file ...]

     Options:
       -help            brief help message
       -man             full documentation

    =head1 OPTIONS

    =over 8

    =item B<-help>

    Print a brief help message and exits.

    =item B<-man>

    Prints the manual page and exits.

    =back

    =head1 DESCRIPTION

    B<This program> will read the given input file(s) and do someting
    useful with the contents thereof.

    =cut

See L<Pod::Usage> for details.

=head2 Storing options in a hash

Sometimes, for example when there are a lot of options, having a
separate variable for each of them can be cumbersome. GetOptions()
supports, as an alternative mechanism, storing options in a hash.

To obtain this, a reference to a hash must be passed I<as the first
argument> to GetOptions(). For each option that is specified on the
command line, the option value will be stored in the hash with the
option name as key. Options that are not actually used on the command
line will not be put in the hash, on other words,
C<exists($h{option})> (or defined()) can be used to test if an option
was used. The drawback is that warnings will be issued if the program
runs under C<use strict> and uses C<$h{option}> without testing with
exists() or defined() first.

    my %h = ();
    GetOptions (\%h, 'length=i');	# will store in $h{length}

For options that take list or hash values, it is necessary to indicate
this by appending an C<@> or C<%> sign after the type:

    GetOptions (\%h, 'colours=s@');	# will push to @{$h{colours}}

To make things more complicated, the hash may contain references to
the actual destinations, for example:

    my $len = 0;
    my %h = ('length' => \$len);
    GetOptions (\%h, 'length=i');	# will store in $len

This example is fully equivalent with:

    my $len = 0;
    GetOptions ('length=i' => \$len);	# will store in $len

Any mixture is possible. For example, the most frequently used options
could be stored in variables while all other options get stored in the
hash:

    my $verbose = 0;			# frequently referred
    my $debug = 0;			# frequently referred
    my %h = ('verbose' => \$verbose, 'debug' => \$debug);
    GetOptions (\%h, 'verbose', 'debug', 'filter', 'size=i');
    if ( $verbose ) { ... }
    if ( exists $h{filter} ) { ... option 'filter' was specified ... }

=head2 Bundling

With bundling it is possible to set several single-character options
at once. For example if C<a>, C<v> and C<x> are all valid options,

    -vax

would set all three.

Getopt::Long supports two levels of bundling. To enable bundling, a
call to Getopt::Long::Configure is required.

The first level of bundling can be enabled with:

    Getopt::Long::Configure ("bundling");

Configured this way, single-character options can be bundled but long
options B<must> always start with a double dash C<--> to avoid
abiguity. For example, when C<vax>, C<a>, C<v> and C<x> are all valid
options,

    -vax

would set C<a>, C<v> and C<x>, but 

    --vax

would set C<vax>.

The second level of bundling lifts this restriction. It can be enabled
with:

    Getopt::Long::Configure ("bundling_override");

Now, C<-vax> would set the option C<vax>.

When any level of bundling is enabled, option values may be inserted
in the bundle. For example:

    -h24w80

is equivalent to

    -h 24 -w 80

When configured for bundling, single-character options are matched
case sensitive while long options are matched case insensitive. To
have the single-character options matched case insensitive as well,
use:

    Getopt::Long::Configure ("bundling", "ignorecase_always");

It goes without saying that bundling can be quite confusing.

=head2 The lonesome dash

Some applications require the option C<-> (that's a lone dash). This
can be achieved by adding an option specification with an empty name:

    GetOptions ('' => \$stdio);

A lone dash on the command line will now be legal, and set options
variable C<$stdio>.

=head2 Argument call-back

A special option 'name' C<<>> can be used to designate a subroutine
to handle non-option arguments. When GetOptions() encounters an
argument that does not look like an option, it will immediately call this
subroutine and passes it the argument as a parameter.

For example:

    my $width = 80;
    sub process { ... }
    GetOptions ('width=i' => \$width, '<>' => \&process);

When applied to the following command line:

    arg1 --width=72 arg2 --width=60 arg3

This will call 
C<process("arg1")> while C<$width> is C<80>, 
C<process("arg2")> while C<$width> is C<72>, and
C<process("arg3")> while C<$width> is C<60>.

This feature requires configuration option B<permute>, see section
L<Configuring Getopt::Long>.


=head1 Configuring Getopt::Long

Getopt::Long can be configured by calling subroutine
Getopt::Long::Configure(). This subroutine takes a list of quoted
strings, each specifying a configuration option to be set, e.g.
C<ignore_case>, or reset, e.g. C<no_ignore_case>. Case does not
matter. Multiple calls to Configure() are possible.

The following options are available:

=over 12

=item default

This option causes all configuration options to be reset to their
default values.

=item auto_abbrev

Allow option names to be abbreviated to uniqueness.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case C<auto_abbrev> is reset.

=item getopt_compat

Allow C<+> to start options.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case C<getopt_compat> is reset.

=item require_order

Whether command line arguments are allowed to be mixed with options.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case C<require_order> is reset.

See also C<permute>, which is the opposite of C<require_order>.

=item permute

Whether command line arguments are allowed to be mixed with options.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case C<permute> is reset.
Note that C<permute> is the opposite of C<require_order>.

If C<permute> is set, this means that 

    --foo arg1 --bar arg2 arg3

is equivalent to

    --foo --bar arg1 arg2 arg3

If an argument call-back routine is specified, C<@ARGV> will always be
empty upon succesful return of GetOptions() since all options have been
processed. The only exception is when C<--> is used:

    --foo arg1 --bar arg2 -- arg3

will call the call-back routine for arg1 and arg2, and terminate
GetOptions() leaving C<"arg2"> in C<@ARGV>.

If C<require_order> is set, options processing
terminates when the first non-option is encountered.

    --foo arg1 --bar arg2 arg3

is equivalent to

    --foo -- arg1 --bar arg2 arg3

=item bundling (default: reset)

Setting this option will allow single-character options to be bundled.
To distinguish bundles from long option names, long options I<must> be
introduced with C<--> and single-character options (and bundles) with
C<->.

Note: resetting C<bundling> also resets C<bundling_override>.

=item bundling_override (default: reset)

If C<bundling_override> is set, bundling is enabled as with
C<bundling> but now long option names override option bundles. 

Note: resetting C<bundling_override> also resets C<bundling>.

B<Note:> Using option bundling can easily lead to unexpected results,
especially when mixing long options and bundles. Caveat emptor.

=item ignore_case  (default: set)

If set, case is ignored when matching long option names. Single
character options will be treated case-sensitive.

Note: resetting C<ignore_case> also resets C<ignore_case_always>.

=item ignore_case_always (default: reset)

When bundling is in effect, case is ignored on single-character
options also. 

Note: resetting C<ignore_case_always> also resets C<ignore_case>.

=item pass_through (default: reset)

Options that are unknown, ambiguous or supplied with an invalid option
value are passed through in C<@ARGV> instead of being flagged as
errors. This makes it possible to write wrapper scripts that process
only part of the user supplied command line arguments, and pass the
remaining options to some other program.

This can be very confusing, especially when C<permute> is also set.

=item prefix

The string that starts options. If a constant string is not
sufficient, see C<prefix_pattern>.

=item prefix_pattern

A Perl pattern that identifies the strings that introduce options.
Default is C<(--|-|\+)> unless environment variable
POSIXLY_CORRECT has been set, in which case it is C<(--|-)>.

=item debug (default: reset)

Enable copious debugging output.

=back

=head1 Return values and Errors

Configuration errors and errors in the option definitions are
signalled using die() and will terminate the calling program unless
the call to Getopt::Long::GetOptions() was embedded in C<eval { ...
}>, or die() was trapped using C<$SIG{__DIE__}>.

A return value of 1 (true) indicates success.

A return status of 0 (false) indicates that the function detected one
or more errors during option parsing. These errors are signalled using
warn() and can be trapped with C<$SIG{__WARN__}>.

Errors that can't happen are signalled using Carp::croak().

=head1 Legacy

The earliest development of C<newgetopt.pl> started in 1990, with Perl
version 4. As a result, its development, and the development of
Getopt::Long, has gone through several stages. Since backward
compatibility has always been extremely important, the current version
of Getopt::Long still supports a lot of constructs that nowadays are
no longer necessary or otherwise unwanted. This section describes
briefly some of these 'features'.

=head2 Default destinations

When no destination is specified for an option, GetOptions will store
the resultant value in a global variable named C<opt_>I<XXX>, where
I<XXX> is the primary name of this option. When a progam executes
under C<use strict> (recommended), these variables must be
pre-declared with our() or C<use vars>.

    our $opt_length = 0;
    GetOptions ('length=i');	# will store in $opt_length

To yield a usable Perl variable, characters that are not part of the
syntax for variables are translated to underscores. For example,
C<--fpp-struct-return> will set the variable
C<$opt_fpp_struct_return>. Note that this variable resides in the
namespace of the calling program, not necessarily C<main>. For
example:

    GetOptions ("size=i", "sizes=i@");

with command line "-size 10 -sizes 24 -sizes 48" will perform the
equivalent of the assignments

    $opt_size = 10;
    @opt_sizes = (24, 48);

=head2 Alternative option starters

A string of alternative option starter characters may be passed as the
first argument (or the first argument after a leading hash reference
argument).

    my $len = 0;
    GetOptions ('/', 'length=i' => $len);

Now the command line may look like:

    /length 24 -- arg

Note that to terminate options processing still requires a double dash
C<-->.

GetOptions() will not interpret a leading C<"<>"> as option starters
if the next argument is a reference. To force C<"<"> and C<">"> as
option starters, use C<"><">. Confusing? Well, B<using a starter
argument is strongly deprecated> anyway.

=head2 Configuration variables

Previous versions of Getopt::Long used variables for the purpose of
configuring. Although manipulating these variables still work, it
is strongly encouraged to use the new C<config> routine. Besides, it
is much easier.

=head1 AUTHOR

Johan Vromans E<lt>jvromans@squirrel.nlE<gt>

=head1 COPYRIGHT AND DISCLAIMER

This program is Copyright 2000,1990 by Johan Vromans.
This program is free software; you can redistribute it and/or
modify it under the terms of the Perl Artistic License or the
GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

If you do not have a copy of the GNU General Public License write to
the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, 
MA 02139, USA.

=cut

# Local Variables:
# mode: perl
# eval: (load-file "pod.el")
# End:
