package Exporter;

require 5.001;

#
# We go to a lot of trouble not to 'require Carp' at file scope,
#  because Carp requires Exporter, and something has to give.
#

$ExportLevel = 0;
$Verbose = 0 unless $Verbose;

sub export {

    # First make import warnings look like they're coming from the "use".
    local $SIG{__WARN__} = sub {
	my $text = shift;
	if ($text =~ s/ at \S*Exporter.pm line \d+.*\n//) {
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

sub export_to_level
{
      my $pkg = shift;
      my ($level, $junk) = (shift, shift);  # need to get rid of first arg
                                            # we know it already.
      my $callpkg = caller($level);
      $pkg->export($callpkg, @_);
}

sub import {
    my $pkg = shift;
    my $callpkg = caller($ExportLevel);
    export $pkg, $callpkg, @_;
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

sub export_tags    { _push_tags((caller)[0], "EXPORT",    \@_) }
sub export_ok_tags { _push_tags((caller)[0], "EXPORT_OK", \@_) }


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
	my $file = $INC{"$pkg.pm"};
	$file &&= " ($file)";
	require Carp;
	Carp::croak("$pkg $wanted required--this is only version $version$file")
    }
    $version;
}

1;

# A simple self test harness. Change 'require Carp' to 'use Carp ()' for testing.
# package main; eval(join('',<DATA>)) or die $@ unless caller;
__END__
package Test;
$INC{'Exporter.pm'} = 1;
@ISA = qw(Exporter);
@EXPORT      = qw(A1 A2 A3 A4 A5);
@EXPORT_OK   = qw(B1 B2 B3 B4 B5);
%EXPORT_TAGS = (T1=>[qw(A1 A2 B1 B2)], T2=>[qw(A1 A2 B3 B4)], T3=>[qw(X3)]);
@EXPORT_FAIL = qw(B4);
Exporter::export_ok_tags('T3', 'unknown_tag');
sub export_fail {
    map { "Test::$_" } @_	# edit symbols just as an example
}

package main;
$Exporter::Verbose = 1;
#import Test;
#import Test qw(X3);		# export ok via export_ok_tags()
#import Test qw(:T1 !A2 /5/ !/3/ B5);
import Test qw(:T2 !B4);
import Test qw(:T2);		# should fail
1;

=head1 NAME

Exporter - Implements default import method for modules

=head1 SYNOPSIS

In module ModuleName.pm:

  package ModuleName;
  require Exporter;
  @ISA = qw(Exporter);

  @EXPORT = qw(...);            # symbols to export by default
  @EXPORT_OK = qw(...);         # symbols to export on request
  %EXPORT_TAGS = tag => [...];  # define names for sets of symbols

In other files which wish to use ModuleName:

  use ModuleName;               # import default symbols into my package

  use ModuleName qw(...);       # import listed symbols into my package

  use ModuleName ();            # do not import any symbols

=head1 DESCRIPTION

The Exporter module implements a default C<import> method which
many modules choose to inherit rather than implement their own.

Perl automatically calls the C<import> method when processing a
C<use> statement for a module. Modules and C<use> are documented
in L<perlfunc> and L<perlmod>. Understanding the concept of
modules and how the C<use> statement operates is important to
understanding the Exporter.

=head2 Selecting What To Export

Do B<not> export method names!

Do B<not> export anything else by default without a good reason!

Exports pollute the namespace of the module user.  If you must export
try to use @EXPORT_OK in preference to @EXPORT and avoid short or
common symbol names to reduce the risk of name clashes.

Generally anything not exported is still accessible from outside the
module using the ModuleName::item_name (or $blessed_ref-E<gt>method)
syntax.  By convention you can use a leading underscore on names to
informally indicate that they are 'internal' and not for public use.

(It is actually possible to get private functions by saying:

  my $subref = sub { ... };
  &$subref;

But there's no way to call that directly as a method, since a method
must have a name in the symbol table.)

As a general rule, if the module is trying to be object oriented
then export nothing. If it's just a collection of functions then
@EXPORT_OK anything but use @EXPORT with caution.

Other module design guidelines can be found in L<perlmod>.

=head2 Specialised Import Lists

If the first entry in an import list begins with !, : or / then the
list is treated as a series of specifications which either add to or
delete from the list of names to import. They are processed left to
right. Specifications are in the form:

    [!]name         This name only
    [!]:DEFAULT     All names in @EXPORT
    [!]:tag         All names in $EXPORT_TAGS{tag} anonymous list
    [!]/pattern/    All names in @EXPORT and @EXPORT_OK which match

A leading ! indicates that matching names should be deleted from the
list of names to import.  If the first specification is a deletion it
is treated as though preceded by :DEFAULT. If you just want to import
extra names in addition to the default set you will still need to
include :DEFAULT explicitly.

e.g., Module.pm defines:

    @EXPORT      = qw(A1 A2 A3 A4 A5);
    @EXPORT_OK   = qw(B1 B2 B3 B4 B5);
    %EXPORT_TAGS = (T1 => [qw(A1 A2 B1 B2)], T2 => [qw(A1 A2 B3 B4)]);

    Note that you cannot use tags in @EXPORT or @EXPORT_OK.
    Names in EXPORT_TAGS must also appear in @EXPORT or @EXPORT_OK.

An application using Module can say something like:

    use Module qw(:DEFAULT :T2 !B3 A3);

Other examples include:

    use Socket qw(!/^[AP]F_/ !SOMAXCONN !SOL_SOCKET);
    use POSIX  qw(:errno_h :termios_h !TCSADRAIN !/^EXIT/);

Remember that most patterns (using //) will need to be anchored
with a leading ^, e.g., C</^EXIT/> rather than C</EXIT/>.

You can say C<BEGIN { $Exporter::Verbose=1 }> to see how the
specifications are being processed and what is actually being imported
into modules.

=head2 Exporting without using Export's import method

Exporter has a special method, 'export_to_level' which is used in situations
where you can't directly call Export's import method. The export_to_level
method looks like:

MyPackage->export_to_level($where_to_export, @what_to_export);

where $where_to_export is an integer telling how far up the calling stack
to export your symbols, and @what_to_export is an array telling what
symbols *to* export (usually this is @_).

For example, suppose that you have a module, A, which already has an
import function:

package A;

@ISA = qw(Exporter);
@EXPORT_OK = qw ($b);

sub import
{
    $A::b = 1;     # not a very useful import method
}

and you want to Export symbol $A::b back to the module that called 
package A. Since Exporter relies on the import method to work, via 
inheritance, as it stands Exporter::import() will never get called. 
Instead, say the following:

package A;
@ISA = qw(Exporter);
@EXPORT_OK = qw ($b);

sub import
{
    $A::b = 1;
    A->export_to_level(1, @_);
}

This will export the symbols one level 'above' the current package - ie: to 
the program or module that used package A. 

Note: Be careful not to modify '@_' at all before you call export_to_level
- or people using your package will get very unexplained results!


=head2 Module Version Checking

The Exporter module will convert an attempt to import a number from a
module into a call to $module_name-E<gt>require_version($value). This can
be used to validate that the version of the module being used is
greater than or equal to the required version.

The Exporter module supplies a default require_version method which
checks the value of $VERSION in the exporting module.

Since the default require_version method treats the $VERSION number as
a simple numeric value it will regard version 1.10 as lower than
1.9. For this reason it is strongly recommended that you use numbers
with at least two decimal places, e.g., 1.09.

=head2 Managing Unknown Symbols

In some situations you may want to prevent certain symbols from being
exported. Typically this applies to extensions which have functions
or constants that may not exist on some systems.

The names of any symbols that cannot be exported should be listed
in the C<@EXPORT_FAIL> array.

If a module attempts to import any of these symbols the Exporter
will give the module an opportunity to handle the situation before
generating an error. The Exporter will call an export_fail method
with a list of the failed symbols:

  @failed_symbols = $module_name->export_fail(@failed_symbols);

If the export_fail method returns an empty list then no error is
recorded and all the requested symbols are exported. If the returned
list is not empty then an error is generated for each symbol and the
export fails. The Exporter provides a default export_fail method which
simply returns the list unchanged.

Uses for the export_fail method include giving better error messages
for some symbols and performing lazy architectural checks (put more
symbols into @EXPORT_FAIL by default and then take them out if someone
actually tries to use them and an expensive check shows that they are
usable on that platform).

=head2 Tag Handling Utility Functions

Since the symbols listed within %EXPORT_TAGS must also appear in either
@EXPORT or @EXPORT_OK, two utility functions are provided which allow
you to easily add tagged sets of symbols to @EXPORT or @EXPORT_OK:

  %EXPORT_TAGS = (foo => [qw(aa bb cc)], bar => [qw(aa cc dd)]);

  Exporter::export_tags('foo');     # add aa, bb and cc to @EXPORT
  Exporter::export_ok_tags('bar');  # add aa, cc and dd to @EXPORT_OK

Any names which are not tags are added to @EXPORT or @EXPORT_OK
unchanged but will trigger a warning (with C<-w>) to avoid misspelt tags
names being silently added to @EXPORT or @EXPORT_OK. Future versions
may make this a fatal error.

=cut
