package Exporter;

=head1 NAME

Exporter::Heavy - Exporter guts

=head1 SYNOPIS

(internal use only)

=head1 DESCRIPTION

No user-serviceable parts inside.

=cut
#
# We go to a lot of trouble not to 'require Carp' at file scope,
#  because Carp requires Exporter, and something has to give.
#

sub heavy_export {

    # First make import warnings look like they're coming from the "use".
    local $SIG{__WARN__} = sub {
	my $text = shift;
	if ($text =~ s/ at \S*Exporter\S*.pm line \d+.*\n//) {
	    require Carp;
	    local $Carp::CarpLevel = 1;	# ignore package calling us too.
	    Carp::carp($text);
	}
	else {
	    warn $text;
	}
    };
    local $SIG{__DIE__} = sub {
	require Carp;
	local $Carp::CarpLevel = 1;	# ignore package calling us too.
	Carp::croak("$_[0]Illegal null symbol in \@${1}::EXPORT")
	    if $_[0] =~ /^Unable to create sub named "(.*?)::"/;
    };

    my($pkg, $callpkg, @imports) = @_;
    my($type, $sym, $oops);
    *exports = *{"${pkg}::EXPORT"};

    if (@imports) {
	if (!%exports) {
	    grep(s/^&//, @exports);
	    @exports{@exports} = (1) x @exports;
	    my $ok = \@{"${pkg}::EXPORT_OK"};
	    if (@$ok) {
		grep(s/^&//, @$ok);
		@exports{@$ok} = (1) x @$ok;
	    }
	}

	if ($imports[0] =~ m#^[/!:]#){
	    my $tagsref = \%{"${pkg}::EXPORT_TAGS"};
	    my $tagdata;
	    my %imports;
	    my($remove, $spec, @names, @allexports);
	    # negated first item implies starting with default set:
	    unshift @imports, ':DEFAULT' if $imports[0] =~ m/^!/;
	    foreach $spec (@imports){
		$remove = $spec =~ s/^!//;

		if ($spec =~ s/^://){
		    if ($spec eq 'DEFAULT'){
			@names = @exports;
		    }
		    elsif ($tagdata = $tagsref->{$spec}) {
			@names = @$tagdata;
		    }
		    else {
			warn qq["$spec" is not defined in %${pkg}::EXPORT_TAGS];
			++$oops;
			next;
		    }
		}
		elsif ($spec =~ m:^/(.*)/$:){
		    my $patn = $1;
		    @allexports = keys %exports unless @allexports; # only do keys once
		    @names = grep(/$patn/, @allexports); # not anchored by default
		}
		else {
		    @names = ($spec); # is a normal symbol name
		}

		warn "Import ".($remove ? "del":"add").": @names "
		    if $Verbose;

		if ($remove) {
		   foreach $sym (@names) { delete $imports{$sym} } 
		}
		else {
		    @imports{@names} = (1) x @names;
		}
	    }
	    @imports = keys %imports;
	}

	foreach $sym (@imports) {
	    if (!$exports{$sym}) {
		if ($sym =~ m/^\d/) {
		    $pkg->require_version($sym);
		    # If the version number was the only thing specified
		    # then we should act as if nothing was specified:
		    if (@imports == 1) {
			@imports = @exports;
			last;
		    }
		    # We need a way to emulate 'use Foo ()' but still
		    # allow an easy version check: "use Foo 1.23, ''";
		    if (@imports == 2 and !$imports[1]) {
			@imports = ();
			last;
		    }
		} elsif ($sym !~ s/^&// || !$exports{$sym}) {
                    require Carp;
		    Carp::carp(qq["$sym" is not exported by the $pkg module]);
		    $oops++;
		}
	    }
	}
	if ($oops) {
	    require Carp;
	    Carp::croak("Can't continue after import errors");
	}
    }
    else {
	@imports = @exports;
    }

    *fail = *{"${pkg}::EXPORT_FAIL"};
    if (@fail) {
	if (!%fail) {
	    # Build cache of symbols. Optimise the lookup by adding
	    # barewords twice... both with and without a leading &.
	    # (Technique could be applied to %exports cache at cost of memory)
	    my @expanded = map { /^\w/ ? ($_, '&'.$_) : $_ } @fail;
	    warn "${pkg}::EXPORT_FAIL cached: @expanded" if $Verbose;
	    @fail{@expanded} = (1) x @expanded;
	}
	my @failed;
	foreach $sym (@imports) { push(@failed, $sym) if $fail{$sym} }
	if (@failed) {
	    @failed = $pkg->export_fail(@failed);
	    foreach $sym (@failed) {
                require Carp;
		Carp::carp(qq["$sym" is not implemented by the $pkg module ],
			"on this architecture");
	    }
	    if (@failed) {
		require Carp;
		Carp::croak("Can't continue after import errors");
	    }
	}
    }

    warn "Importing into $callpkg from $pkg: ",
		join(", ",sort @imports) if $Verbose;

    foreach $sym (@imports) {
	# shortcut for the common case of no type character
	(*{"${callpkg}::$sym"} = \&{"${pkg}::$sym"}, next)
	    unless $sym =~ s/^(\W)//;
	$type = $1;
	*{"${callpkg}::$sym"} =
	    $type eq '&' ? \&{"${pkg}::$sym"} :
	    $type eq '$' ? \${"${pkg}::$sym"} :
	    $type eq '@' ? \@{"${pkg}::$sym"} :
	    $type eq '%' ? \%{"${pkg}::$sym"} :
	    $type eq '*' ?  *{"${pkg}::$sym"} :
	    do { require Carp; Carp::croak("Can't export symbol: $type$sym") };
    }
}

sub heavy_export_to_level
{
      my $pkg = shift;
      my $level = shift;
      (undef) = shift;			# XXX redundant arg
      my $callpkg = caller($level);
      $pkg->export($callpkg, @_);
}

# Utility functions

sub _push_tags {
    my($pkg, $var, $syms) = @_;
    my $nontag;
    *export_tags = \%{"${pkg}::EXPORT_TAGS"};
    push(@{"${pkg}::$var"},
	map { $export_tags{$_} ? @{$export_tags{$_}} : scalar(++$nontag,$_) }
		(@$syms) ? @$syms : keys %export_tags);
    if ($nontag and $^W) {
	# This may change to a die one day
	require Carp;
	Carp::carp("Some names are not tags");
    }
}

# Default methods

sub export_fail {
    my $self = shift;
    @_;
}

sub require_version {
    my($self, $wanted) = @_;
    my $pkg = ref $self || $self;
    my $version = ${"${pkg}::VERSION"};
    if (!$version or $version < $wanted) {
	$version ||= "(undef)";
	    # %INC contains slashes, but $pkg contains double-colons.
	my $file = (map {s,::,/,g; $INC{$_}} "$pkg.pm")[0];
	$file &&= " ($file)";
	require Carp;
	Carp::croak("$pkg $wanted required--this is only version $version$file")
    }
    $version;
}

1;
