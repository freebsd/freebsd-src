package Exporter;

require 5.001;

$ExportLevel = 0;
$Verbose ||= 0;
$VERSION = '5.562';

sub export_to_level {
  require Exporter::Heavy;
  goto &heavy_export_to_level;
}

sub export {
  require Exporter::Heavy;
  goto &heavy_export;
}

sub export_tags {
  require Exporter::Heavy;
  _push_tags((caller)[0], "EXPORT",    \@_);
}

sub export_ok_tags {
  require Exporter::Heavy;
  _push_tags((caller)[0], "EXPORT_OK", \@_);
}

sub import {
  my $pkg = shift;
  my $callpkg = caller($ExportLevel);
  *exports = *{"$pkg\::EXPORT"};
  # We *need* to treat @{"$pkg\::EXPORT_FAIL"} since Carp uses it :-(
  *fail = *{"$pkg\::EXPORT_FAIL"};
  return export $pkg, $callpkg, @_
    if $Verbose or $Debug or @fail > 1;
  my $args = @_ or @_ = @exports;
  
  if ($args and not %exports) {
    foreach my $sym (@exports, @{"$pkg\::EXPORT_OK"}) {
      $sym =~ s/^&//;
      $exports{$sym} = 1;
    }
  }
  if ($Verbose or $Debug 
      or grep {/\W/ or $args and not exists $exports{$_}
	       or @fail and $_ eq $fail[0]
	       or (@{"$pkg\::EXPORT_OK"} 
		   and $_ eq ${"$pkg\::EXPORT_OK"}[0])} @_) {
    return export $pkg, $callpkg, ($args ? @_ : ());
  }
  #local $SIG{__WARN__} = sub {require Carp; goto &Carp::carp};
  local $SIG{__WARN__} = 
	sub {require Carp; local $Carp::CarpLevel = 1; &Carp::carp};
  foreach $sym (@_) {
    # shortcut for the common case of no type character
    *{"$callpkg\::$sym"} = \&{"$pkg\::$sym"};
  }
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

=head2 How to Export

The arrays C<@EXPORT> and C<@EXPORT_OK> in a module hold lists of
symbols that are going to be exported into the users name space by
default, or which they can request to be exported, respectively.  The
symbols can represent functions, scalars, arrays, hashes, or typeglobs.
The symbols must be given by full name with the exception that the
ampersand in front of a function is optional, e.g.

    @EXPORT    = qw(afunc $scalar @array);   # afunc is a function
    @EXPORT_OK = qw(&bfunc %hash *typeglob); # explicit prefix on &bfunc

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

MyPackage->export_to_level($where_to_export, $package, @what_to_export);

where $where_to_export is an integer telling how far up the calling stack
to export your symbols, and @what_to_export is an array telling what
symbols *to* export (usually this is @_).  The $package argument is
currently unused.

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
