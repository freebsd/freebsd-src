package B::Lint;

=head1 NAME

B::Lint - Perl lint

=head1 SYNOPSIS

perl -MO=Lint[,OPTIONS] foo.pl

=head1 DESCRIPTION

The B::Lint module is equivalent to an extended version of the B<-w>
option of B<perl>. It is named after the program B<lint> which carries
out a similar process for C programs.

=head1 OPTIONS AND LINT CHECKS

Option words are separated by commas (not whitespace) and follow the
usual conventions of compiler backend options. Following any options
(indicated by a leading B<->) come lint check arguments. Each such
argument (apart from the special B<all> and B<none> options) is a
word representing one possible lint check (turning on that check) or
is B<no-foo> (turning off that check). Before processing the check
arguments, a standard list of checks is turned on. Later options
override earlier ones. Available options are:

=over 8

=item B<context>

Produces a warning whenever an array is used in an implicit scalar
context. For example, both of the lines

    $foo = length(@bar);
    $foo = @bar;
will elicit a warning. Using an explicit B<scalar()> silences the
warning. For example,

    $foo = scalar(@bar);

=item B<implicit-read> and B<implicit-write>

These options produce a warning whenever an operation implicitly
reads or (respectively) writes to one of Perl's special variables.
For example, B<implicit-read> will warn about these:

    /foo/;

and B<implicit-write> will warn about these:

    s/foo/bar/;

Both B<implicit-read> and B<implicit-write> warn about this:

    for (@a) { ... }

=item B<dollar-underscore>

This option warns whenever $_ is used either explicitly anywhere or
as the implicit argument of a B<print> statement.

=item B<private-names>

This option warns on each use of any variable, subroutine or
method name that lives in a non-current package but begins with
an underscore ("_"). Warnings aren't issued for the special case
of the single character name "_" by itself (e.g. $_ and @_).

=item B<undefined-subs>

This option warns whenever an undefined subroutine is invoked.
This option will only catch explicitly invoked subroutines such
as C<foo()> and not indirect invocations such as C<&$subref()>
or C<$obj-E<gt>meth()>. Note that some programs or modules delay
definition of subs until runtime by means of the AUTOLOAD
mechanism.

=item B<regexp-variables>

This option warns whenever one of the regexp variables $', $& or
$' is used. Any occurrence of any of these variables in your
program can slow your whole program down. See L<perlre> for
details.

=item B<all>

Turn all warnings on.

=item B<none>

Turn all warnings off.

=back

=head1 NON LINT-CHECK OPTIONS

=over 8

=item B<-u Package>

Normally, Lint only checks the main code of the program together
with all subs defined in package main. The B<-u> option lets you
include other package names whose subs are then checked by Lint.

=back

=head1 BUGS

This is only a very preliminary version.

=head1 AUTHOR

Malcolm Beattie, mbeattie@sable.ox.ac.uk.

=cut

use strict;
use B qw(walkoptree main_root walksymtable svref_2object parents
         OPf_WANT_LIST OPf_WANT OPf_STACKED G_ARRAY
        );

my $file = "unknown";		# shadows current filename
my $line = 0;			# shadows current line number
my $curstash = "main";		# shadows current stash

# Lint checks
my %check;
my %implies_ok_context;
BEGIN {
    map($implies_ok_context{$_}++,
	qw(scalar av2arylen aelem aslice helem hslice
	   keys values hslice defined undef delete));
}

# Lint checks turned on by default
my @default_checks = qw(context);

my %valid_check;
# All valid checks
BEGIN {
    map($valid_check{$_}++,
	qw(context implicit_read implicit_write dollar_underscore
	   private_names undefined_subs regexp_variables));
}

# Debugging options
my ($debug_op);

my %done_cv;		# used to mark which subs have already been linted
my @extra_packages;	# Lint checks mainline code and all subs which are
			# in main:: or in one of these packages.

sub warning {
    my $format = (@_ < 2) ? "%s" : shift;
    warn sprintf("$format at %s line %d\n", @_, $file, $line);
}

# This gimme can't cope with context that's only determined
# at runtime via dowantarray().
sub gimme {
    my $op = shift;
    my $flags = $op->flags;
    if ($flags & OPf_WANT) {
	return(($flags & OPf_WANT_LIST) ? 1 : 0);
    }
    return undef;
}

sub B::OP::lint {}

sub B::COP::lint {
    my $op = shift;
    if ($op->name eq "nextstate") {
	$file = $op->file;
	$line = $op->line;
	$curstash = $op->stash->NAME;
    }
}

sub B::UNOP::lint {
    my $op = shift;
    my $opname = $op->name;
    if ($check{context} && ($opname eq "rv2av" || $opname eq "rv2hv")) {
	my $parent = parents->[0];
	my $pname = $parent->name;
	return if gimme($op) || $implies_ok_context{$pname};
	# Two special cases to deal with: "foreach (@foo)" and "delete $a{$b}"
	# null out the parent so we have to check for a parent of pp_null and
	# a grandparent of pp_enteriter or pp_delete
	if ($pname eq "null") {
	    my $gpname = parents->[1]->name;
	    return if $gpname eq "enteriter" || $gpname eq "delete";
	}
	warning("Implicit scalar context for %s in %s",
		$opname eq "rv2av" ? "array" : "hash", $parent->desc);
    }
    if ($check{private_names} && $opname eq "method") {
	my $methop = $op->first;
	if ($methop->name eq "const") {
	    my $method = $methop->sv->PV;
	    if ($method =~ /^_/ && !defined(&{"$curstash\::$method"})) {
		warning("Illegal reference to private method name $method");
	    }
	}
    }
}

sub B::PMOP::lint {
    my $op = shift;
    if ($check{implicit_read}) {
	if ($op->name eq "match" && !($op->flags & OPf_STACKED)) {
	    warning('Implicit match on $_');
	}
    }
    if ($check{implicit_write}) {
	if ($op->name eq "subst" && !($op->flags & OPf_STACKED)) {
	    warning('Implicit substitution on $_');
	}
    }
}

sub B::LOOP::lint {
    my $op = shift;
    if ($check{implicit_read} || $check{implicit_write}) {
	if ($op->name eq "enteriter") {
	    my $last = $op->last;
	    if ($last->name eq "gv" && $last->gv->NAME eq "_") {
		warning('Implicit use of $_ in foreach');
	    }
	}
    }
}

sub B::SVOP::lint {
    my $op = shift;
    if ($check{dollar_underscore} && $op->name eq "gvsv"
	&& $op->gv->NAME eq "_")
    {
	warning('Use of $_');
    }
    if ($check{private_names}) {
	my $opname = $op->name;
	if ($opname eq "gv" || $opname eq "gvsv") {
	    my $gv = $op->gv;
	    if ($gv->NAME =~ /^_./ && $gv->STASH->NAME ne $curstash) {
		warning('Illegal reference to private name %s', $gv->NAME);
	    }
	}
    }
    if ($check{undefined_subs}) {
	if ($op->name eq "gv"
	    && $op->next->name eq "entersub")
	{
	    my $gv = $op->gv;
	    my $subname = $gv->STASH->NAME . "::" . $gv->NAME;
	    no strict 'refs';
	    if (!defined(&$subname)) {
		$subname =~ s/^main:://;
		warning('Undefined subroutine %s called', $subname);
	    }
	}
    }
    if ($check{regexp_variables} && $op->name eq "gvsv") {
	my $name = $op->gv->NAME;
	if ($name =~ /^[&'`]$/) {
	    warning('Use of regexp variable $%s', $name);
	}
    }
}

sub B::GV::lintcv {
    my $gv = shift;
    my $cv = $gv->CV;
    #warn sprintf("lintcv: %s::%s (done=%d)\n",
    #		 $gv->STASH->NAME, $gv->NAME, $done_cv{$$cv});#debug
    return if !$$cv || $done_cv{$$cv}++;
    my $root = $cv->ROOT;
    #warn "    root = $root (0x$$root)\n";#debug
    walkoptree($root, "lint") if $$root;
}

sub do_lint {
    my %search_pack;
    walkoptree(main_root, "lint") if ${main_root()};
    
    # Now do subs in main
    no strict qw(vars refs);
    my $sym;
    local(*glob);
    while (($sym, *glob) = each %{"main::"}) {
	#warn "Trying $sym\n";#debug
	svref_2object(\*glob)->EGV->lintcv unless $sym =~ /::$/;
    }

    # Now do subs in non-main packages given by -u options
    map { $search_pack{$_} = 1 } @extra_packages;
    walksymtable(\%{"main::"}, "lintcv", sub {
	my $package = shift;
	$package =~ s/::$//;
	#warn "Considering $package\n";#debug
	return exists $search_pack{$package};
    });
}

sub compile {
    my @options = @_;
    my ($option, $opt, $arg);
    # Turn on default lint checks
    for $opt (@default_checks) {
	$check{$opt} = 1;
    }
  OPTION:
    while ($option = shift @options) {
	if ($option =~ /^-(.)(.*)/) {
	    $opt = $1;
	    $arg = $2;
	} else {
	    unshift @options, $option;
	    last OPTION;
	}
	if ($opt eq "-" && $arg eq "-") {
	    shift @options;
	    last OPTION;
	} elsif ($opt eq "D") {
            $arg ||= shift @options;
	    foreach $arg (split(//, $arg)) {
		if ($arg eq "o") {
		    B->debug(1);
		} elsif ($arg eq "O") {
		    $debug_op = 1;
		}
	    }
	} elsif ($opt eq "u") {
	    $arg ||= shift @options;
	    push(@extra_packages, $arg);
	}
    }
    foreach $opt (@default_checks, @options) {
	$opt =~ tr/-/_/;
	if ($opt eq "all") {
	    %check = %valid_check;
	}
	elsif ($opt eq "none") {
	    %check = ();
	}
	else {
	    if ($opt =~ s/^no-//) {
		$check{$opt} = 0;
	    }
	    else {
		$check{$opt} = 1;
	    }
	    warn "No such check: $opt\n" unless defined $valid_check{$opt};
	}
    }
    # Remaining arguments are things to check
    
    return \&do_lint;
}

1;
