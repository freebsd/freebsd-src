#      B.pm
#
#      Copyright (c) 1996, 1997, 1998 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
#
package B;
use XSLoader ();
require Exporter;
@ISA = qw(Exporter);

# walkoptree_slow comes from B.pm (you are there),
# walkoptree comes from B.xs
@EXPORT_OK = qw(minus_c ppname save_BEGINs
		class peekop cast_I32 cstring cchar hash threadsv_names
		main_root main_start main_cv svref_2object opnumber
		amagic_generation
		walkoptree_slow walkoptree walkoptree_exec walksymtable
		parents comppadlist sv_undef compile_stats timing_info
		begin_av init_av end_av);

sub OPf_KIDS ();
use strict;
@B::SV::ISA = 'B::OBJECT';
@B::NULL::ISA = 'B::SV';
@B::PV::ISA = 'B::SV';
@B::IV::ISA = 'B::SV';
@B::NV::ISA = 'B::IV';
@B::RV::ISA = 'B::SV';
@B::PVIV::ISA = qw(B::PV B::IV);
@B::PVNV::ISA = qw(B::PV B::NV);
@B::PVMG::ISA = 'B::PVNV';
@B::PVLV::ISA = 'B::PVMG';
@B::BM::ISA = 'B::PVMG';
@B::AV::ISA = 'B::PVMG';
@B::GV::ISA = 'B::PVMG';
@B::HV::ISA = 'B::PVMG';
@B::CV::ISA = 'B::PVMG';
@B::IO::ISA = 'B::PVMG';
@B::FM::ISA = 'B::CV';

@B::OP::ISA = 'B::OBJECT';
@B::UNOP::ISA = 'B::OP';
@B::BINOP::ISA = 'B::UNOP';
@B::LOGOP::ISA = 'B::UNOP';
@B::LISTOP::ISA = 'B::BINOP';
@B::SVOP::ISA = 'B::OP';
@B::PADOP::ISA = 'B::OP';
@B::PVOP::ISA = 'B::OP';
@B::CVOP::ISA = 'B::OP';
@B::LOOP::ISA = 'B::LISTOP';
@B::PMOP::ISA = 'B::LISTOP';
@B::COP::ISA = 'B::OP';

@B::SPECIAL::ISA = 'B::OBJECT';

{
    # Stop "-w" from complaining about the lack of a real B::OBJECT class
    package B::OBJECT;
}

sub B::GV::SAFENAME {
  my $name = (shift())->NAME;

  # The regex below corresponds to the isCONTROLVAR macro
  # from toke.c

  $name =~ s/^([\cA-\cZ\c\\c[\c]\c?\c_\c^])/"^".chr(64 ^ ord($1))/e;
  return $name;
}

sub B::IV::int_value {
  my ($self) = @_;
  return (($self->FLAGS() & SVf_IVisUV()) ? $self->UVX : $self->IV);
}

my $debug;
my $op_count = 0;
my @parents = ();

sub debug {
    my ($class, $value) = @_;
    $debug = $value;
    walkoptree_debug($value);
}

sub class {
    my $obj = shift;
    my $name = ref $obj;
    $name =~ s/^.*:://;
    return $name;
}

sub parents { \@parents }

# For debugging
sub peekop {
    my $op = shift;
    return sprintf("%s (0x%x) %s", class($op), $$op, $op->name);
}

sub walkoptree_slow {
    my($op, $method, $level) = @_;
    $op_count++; # just for statistics
    $level ||= 0;
    warn(sprintf("walkoptree: %d. %s\n", $level, peekop($op))) if $debug;
    $op->$method($level);
    if ($$op && ($op->flags & OPf_KIDS)) {
	my $kid;
	unshift(@parents, $op);
	for ($kid = $op->first; $$kid; $kid = $kid->sibling) {
	    walkoptree_slow($kid, $method, $level + 1);
	}
	shift @parents;
    }
}

sub compile_stats {
    return "Total number of OPs processed: $op_count\n";
}

sub timing_info {
    my ($sec, $min, $hr) = localtime;
    my ($user, $sys) = times;
    sprintf("%02d:%02d:%02d user=$user sys=$sys",
	    $hr, $min, $sec, $user, $sys);
}

my %symtable;

sub clearsym {
    %symtable = ();
}

sub savesym {
    my ($obj, $value) = @_;
#    warn(sprintf("savesym: sym_%x => %s\n", $$obj, $value)); # debug
    $symtable{sprintf("sym_%x", $$obj)} = $value;
}

sub objsym {
    my $obj = shift;
    return $symtable{sprintf("sym_%x", $$obj)};
}

sub walkoptree_exec {
    my ($op, $method, $level) = @_;
    $level ||= 0;
    my ($sym, $ppname);
    my $prefix = "    " x $level;
    for (; $$op; $op = $op->next) {
	$sym = objsym($op);
	if (defined($sym)) {
	    print $prefix, "goto $sym\n";
	    return;
	}
	savesym($op, sprintf("%s (0x%lx)", class($op), $$op));
	$op->$method($level);
	$ppname = $op->name;
	if ($ppname =~
	    /^(or|and|mapwhile|grepwhile|entertry|range|cond_expr)$/)
	{
	    print $prefix, uc($1), " => {\n";
	    walkoptree_exec($op->other, $method, $level + 1);
	    print $prefix, "}\n";
	} elsif ($ppname eq "match" || $ppname eq "subst") {
	    my $pmreplstart = $op->pmreplstart;
	    if ($$pmreplstart) {
		print $prefix, "PMREPLSTART => {\n";
		walkoptree_exec($pmreplstart, $method, $level + 1);
		print $prefix, "}\n";
	    }
	} elsif ($ppname eq "substcont") {
	    print $prefix, "SUBSTCONT => {\n";
	    walkoptree_exec($op->other->pmreplstart, $method, $level + 1);
	    print $prefix, "}\n";
	    $op = $op->other;
	} elsif ($ppname eq "enterloop") {
	    print $prefix, "REDO => {\n";
	    walkoptree_exec($op->redoop, $method, $level + 1);
	    print $prefix, "}\n", $prefix, "NEXT => {\n";
	    walkoptree_exec($op->nextop, $method, $level + 1);
	    print $prefix, "}\n", $prefix, "LAST => {\n";
	    walkoptree_exec($op->lastop,  $method, $level + 1);
	    print $prefix, "}\n";
	} elsif ($ppname eq "subst") {
	    my $replstart = $op->pmreplstart;
	    if ($$replstart) {
		print $prefix, "SUBST => {\n";
		walkoptree_exec($replstart, $method, $level + 1);
		print $prefix, "}\n";
	    }
	}
    }
}

sub walksymtable {
    my ($symref, $method, $recurse, $prefix) = @_;
    my $sym;
    my $ref;
    no strict 'vars';
    local(*glob);
    $prefix = '' unless defined $prefix;
    while (($sym, $ref) = each %$symref) {
	*glob = "*main::".$prefix.$sym;
	if ($sym =~ /::$/) {
	    $sym = $prefix . $sym;
	    if ($sym ne "main::" && $sym ne "<none>::" && &$recurse($sym)) {
		walksymtable(\%glob, $method, $recurse, $sym);
	    }
	} else {
	    svref_2object(\*glob)->EGV->$method();
	}
    }
}

{
    package B::Section;
    my $output_fh;
    my %sections;
    
    sub new {
	my ($class, $section, $symtable, $default) = @_;
	$output_fh ||= FileHandle->new_tmpfile;
	my $obj = bless [-1, $section, $symtable, $default], $class;
	$sections{$section} = $obj;
	return $obj;
    }
    
    sub get {
	my ($class, $section) = @_;
	return $sections{$section};
    }

    sub add {
	my $section = shift;
	while (defined($_ = shift)) {
	    print $output_fh "$section->[1]\t$_\n";
	    $section->[0]++;
	}
    }

    sub index {
	my $section = shift;
	return $section->[0];
    }

    sub name {
	my $section = shift;
	return $section->[1];
    }

    sub symtable {
	my $section = shift;
	return $section->[2];
    }
	
    sub default {
	my $section = shift;
	return $section->[3];
    }
	
    sub output {
	my ($section, $fh, $format) = @_;
	my $name = $section->name;
	my $sym = $section->symtable || {};
	my $default = $section->default;

	seek($output_fh, 0, 0);
	while (<$output_fh>) {
	    chomp;
	    s/^(.*?)\t//;
	    if ($1 eq $name) {
		s{(s\\_[0-9a-f]+)} {
		    exists($sym->{$1}) ? $sym->{$1} : $default;
		}ge;
		printf $fh $format, $_;
	    }
	}
    }
}

XSLoader::load 'B';

1;

__END__

=head1 NAME

B - The Perl Compiler

=head1 SYNOPSIS

	use B;

=head1 DESCRIPTION

The C<B> module supplies classes which allow a Perl program to delve
into its own innards. It is the module used to implement the
"backends" of the Perl compiler. Usage of the compiler does not
require knowledge of this module: see the F<O> module for the
user-visible part. The C<B> module is of use to those who want to
write new compiler backends. This documentation assumes that the
reader knows a fair amount about perl's internals including such
things as SVs, OPs and the internal symbol table and syntax tree
of a program.

=head1 OVERVIEW OF CLASSES

The C structures used by Perl's internals to hold SV and OP
information (PVIV, AV, HV, ..., OP, SVOP, UNOP, ...) are modelled on a
class hierarchy and the C<B> module gives access to them via a true
object hierarchy. Structure fields which point to other objects
(whether types of SV or types of OP) are represented by the C<B>
module as Perl objects of the appropriate class. The bulk of the C<B>
module is the methods for accessing fields of these structures. Note
that all access is read-only: you cannot modify the internals by
using this module.

=head2 SV-RELATED CLASSES

B::IV, B::NV, B::RV, B::PV, B::PVIV, B::PVNV, B::PVMG, B::BM, B::PVLV,
B::AV, B::HV, B::CV, B::GV, B::FM, B::IO. These classes correspond in
the obvious way to the underlying C structures of similar names. The
inheritance hierarchy mimics the underlying C "inheritance". Access
methods correspond to the underlying C macros for field access,
usually with the leading "class indication" prefix removed (Sv, Av,
Hv, ...). The leading prefix is only left in cases where its removal
would cause a clash in method name. For example, C<GvREFCNT> stays
as-is since its abbreviation would clash with the "superclass" method
C<REFCNT> (corresponding to the C function C<SvREFCNT>).

=head2 B::SV METHODS

=over 4

=item REFCNT

=item FLAGS

=back

=head2 B::IV METHODS

=over 4

=item IV

Returns the value of the IV, I<interpreted as
a signed integer>. This will be misleading
if C<FLAGS & SVf_IVisUV>. Perhaps you want the
C<int_value> method instead?

=item IVX

=item UVX

=item int_value

This method returns the value of the IV as an integer.
It differs from C<IV> in that it returns the correct
value regardless of whether it's stored signed or
unsigned.

=item needs64bits

=item packiv

=back

=head2 B::NV METHODS

=over 4

=item NV

=item NVX

=back

=head2 B::RV METHODS

=over 4

=item RV

=back

=head2 B::PV METHODS

=over 4

=item PV

This method is the one you usually want. It constructs a
string using the length and offset information in the struct:
for ordinary scalars it will return the string that you'd see
from Perl, even if it contains null characters.

=item PVX

This method is less often useful. It assumes that the string
stored in the struct is null-terminated, and disregards the
length information.

It is the appropriate method to use if you need to get the name
of a lexical variable from a padname array. Lexical variable names
are always stored with a null terminator, and the length field
(SvCUR) is overloaded for other purposes and can't be relied on here.

=back

=head2 B::PVMG METHODS

=over 4

=item MAGIC

=item SvSTASH

=back

=head2 B::MAGIC METHODS

=over 4

=item MOREMAGIC

=item PRIVATE

=item TYPE

=item FLAGS

=item OBJ

=item PTR

=back

=head2 B::PVLV METHODS

=over 4

=item TARGOFF

=item TARGLEN

=item TYPE

=item TARG

=back

=head2 B::BM METHODS

=over 4

=item USEFUL

=item PREVIOUS

=item RARE

=item TABLE

=back

=head2 B::GV METHODS

=over 4

=item is_empty

This method returns TRUE if the GP field of the GV is NULL.

=item NAME

=item SAFENAME

This method returns the name of the glob, but if the first
character of the name is a control character, then it converts
it to ^X first, so that *^G would return "^G" rather than "\cG".

It's useful if you want to print out the name of a variable.
If you restrict yourself to globs which exist at compile-time
then the result ought to be unambiguous, because code like
C<${"^G"} = 1> is compiled as two ops - a constant string and
a dereference (rv2gv) - so that the glob is created at runtime.

If you're working with globs at runtime, and need to disambiguate
*^G from *{"^G"}, then you should use the raw NAME method.

=item STASH

=item SV

=item IO

=item FORM

=item AV

=item HV

=item EGV

=item CV

=item CVGEN

=item LINE

=item FILE

=item FILEGV

=item GvREFCNT

=item FLAGS

=back

=head2 B::IO METHODS

=over 4

=item LINES

=item PAGE

=item PAGE_LEN

=item LINES_LEFT

=item TOP_NAME

=item TOP_GV

=item FMT_NAME

=item FMT_GV

=item BOTTOM_NAME

=item BOTTOM_GV

=item SUBPROCESS

=item IoTYPE

=item IoFLAGS

=back

=head2 B::AV METHODS

=over 4

=item FILL

=item MAX

=item OFF

=item ARRAY

=item AvFLAGS

=back

=head2 B::CV METHODS

=over 4

=item STASH

=item START

=item ROOT

=item GV

=item FILE

=item DEPTH

=item PADLIST

=item OUTSIDE

=item XSUB

=item XSUBANY

=item CvFLAGS

=back

=head2 B::HV METHODS

=over 4

=item FILL

=item MAX

=item KEYS

=item RITER

=item NAME

=item PMROOT

=item ARRAY

=back

=head2 OP-RELATED CLASSES

B::OP, B::UNOP, B::BINOP, B::LOGOP, B::LISTOP, B::PMOP,
B::SVOP, B::PADOP, B::PVOP, B::CVOP, B::LOOP, B::COP.
These classes correspond in
the obvious way to the underlying C structures of similar names. The
inheritance hierarchy mimics the underlying C "inheritance". Access
methods correspond to the underlying C structre field names, with the
leading "class indication" prefix removed (op_).

=head2 B::OP METHODS

=over 4

=item next

=item sibling

=item name

This returns the op name as a string (e.g. "add", "rv2av").

=item ppaddr

This returns the function name as a string (e.g. "PL_ppaddr[OP_ADD]",
"PL_ppaddr[OP_RV2AV]").

=item desc

This returns the op description from the global C PL_op_desc array
(e.g. "addition" "array deref").

=item targ

=item type

=item seq

=item flags

=item private

=back

=head2 B::UNOP METHOD

=over 4

=item first

=back

=head2 B::BINOP METHOD

=over 4

=item last

=back

=head2 B::LOGOP METHOD

=over 4

=item other

=back

=head2 B::LISTOP METHOD

=over 4

=item children

=back

=head2 B::PMOP METHODS

=over 4

=item pmreplroot

=item pmreplstart

=item pmnext

=item pmregexp

=item pmflags

=item pmpermflags

=item precomp

=back

=head2 B::SVOP METHOD

=over 4

=item sv

=item gv

=back

=head2 B::PADOP METHOD

=over 4

=item padix

=back

=head2 B::PVOP METHOD

=over 4

=item pv

=back

=head2 B::LOOP METHODS

=over 4

=item redoop

=item nextop

=item lastop

=back

=head2 B::COP METHODS

=over 4

=item label

=item stash

=item file

=item cop_seq

=item arybase

=item line

=back

=head1 FUNCTIONS EXPORTED BY C<B>

The C<B> module exports a variety of functions: some are simple
utility functions, others provide a Perl program with a way to
get an initial "handle" on an internal object.

=over 4

=item main_cv

Return the (faked) CV corresponding to the main part of the Perl
program.

=item init_av

Returns the AV object (i.e. in class B::AV) representing INIT blocks.

=item main_root

Returns the root op (i.e. an object in the appropriate B::OP-derived
class) of the main part of the Perl program.

=item main_start

Returns the starting op of the main part of the Perl program.

=item comppadlist

Returns the AV object (i.e. in class B::AV) of the global comppadlist.

=item sv_undef

Returns the SV object corresponding to the C variable C<sv_undef>.

=item sv_yes

Returns the SV object corresponding to the C variable C<sv_yes>.

=item sv_no

Returns the SV object corresponding to the C variable C<sv_no>.

=item amagic_generation

Returns the SV object corresponding to the C variable C<amagic_generation>.

=item walkoptree(OP, METHOD)

Does a tree-walk of the syntax tree based at OP and calls METHOD on
each op it visits. Each node is visited before its children. If
C<walkoptree_debug> (q.v.) has been called to turn debugging on then
the method C<walkoptree_debug> is called on each op before METHOD is
called.

=item walkoptree_debug(DEBUG)

Returns the current debugging flag for C<walkoptree>. If the optional
DEBUG argument is non-zero, it sets the debugging flag to that. See
the description of C<walkoptree> above for what the debugging flag
does.

=item walksymtable(SYMREF, METHOD, RECURSE)

Walk the symbol table starting at SYMREF and call METHOD on each
symbol visited. When the walk reached package symbols "Foo::" it
invokes RECURSE and only recurses into the package if that sub
returns true.

=item svref_2object(SV)

Takes any Perl variable and turns it into an object in the
appropriate B::OP-derived or B::SV-derived class. Apart from functions
such as C<main_root>, this is the primary way to get an initial
"handle" on a internal perl data structure which can then be followed
with the other access methods.

=item ppname(OPNUM)

Return the PP function name (e.g. "pp_add") of op number OPNUM.

=item hash(STR)

Returns a string in the form "0x..." representing the value of the
internal hash function used by perl on string STR.

=item cast_I32(I)

Casts I to the internal I32 type used by that perl.


=item minus_c

Does the equivalent of the C<-c> command-line option. Obviously, this
is only useful in a BEGIN block or else the flag is set too late.


=item cstring(STR)

Returns a double-quote-surrounded escaped version of STR which can
be used as a string in C source code.

=item class(OBJ)

Returns the class of an object without the part of the classname
preceding the first "::". This is used to turn "B::UNOP" into
"UNOP" for example.

=item threadsv_names

In a perl compiled for threads, this returns a list of the special
per-thread threadsv variables.

=back

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
