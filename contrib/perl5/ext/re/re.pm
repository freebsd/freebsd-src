package re;

$VERSION = 0.02;

=head1 NAME

re - Perl pragma to alter regular expression behaviour

=head1 SYNOPSIS

    use re 'taint';
    ($x) = ($^X =~ /^(.*)$/s);     # $x is tainted here

    $pat = '(?{ $foo = 1 })';
    use re 'eval';
    /foo${pat}bar/;		   # won't fail (when not under -T switch)

    {
	no re 'taint';		   # the default
	($x) = ($^X =~ /^(.*)$/s); # $x is not tainted here

	no re 'eval';		   # the default
	/foo${pat}bar/;		   # disallowed (with or without -T switch)
    }

    use re 'debug';		   # NOT lexically scoped (as others are)
    /^(.*)$/s;			   # output debugging info during
    				   #     compile and run time

    use re 'debugcolor';	   # same as 'debug', but with colored output
    ...

(We use $^X in these examples because it's tainted by default.)

=head1 DESCRIPTION

When C<use re 'taint'> is in effect, and a tainted string is the target
of a regex, the regex memories (or values returned by the m// operator
in list context) are tainted.  This feature is useful when regex operations
on tainted data aren't meant to extract safe substrings, but to perform
other transformations.

When C<use re 'eval'> is in effect, a regex is allowed to contain
C<(?{ ... })> zero-width assertions even if the regex contains
variable interpolation.  This is normally disallowed, since it is a 
potential security risk.  Note that this pragma is ignored when the regular
expression is obtained from tainted data, i.e.  evaluation is always
disallowed with tainted regular expressions.  See L<perlre/(?{ code })>.

For the purpose of this pragma, interpolation of precompiled regular 
expressions (i.e., the result of C<qr//>) is I<not> considered variable
interpolation.  Thus:

    /foo${pat}bar/

I<is> allowed if $pat is a precompiled regular expression, even 
if $pat contains C<(?{ ... })> assertions.

When C<use re 'debug'> is in effect, perl emits debugging messages when 
compiling and using regular expressions.  The output is the same as that
obtained by running a C<-DDEBUGGING>-enabled perl interpreter with the
B<-Dr> switch. It may be quite voluminous depending on the complexity
of the match.  Using C<debugcolor> instead of C<debug> enables a
form of output that can be used to get a colorful display on terminals
that understand termcap color sequences.  Set C<$ENV{PERL_RE_TC}> to a
comma-separated list of C<termcap> properties to use for highlighting
strings on/off, pre-point part on/off.  
See L<perldebug/"Debugging regular expressions"> for additional info.

The directive C<use re 'debug'> is I<not lexically scoped>, as the
other directives are.  It has both compile-time and run-time effects.

See L<perlmodlib/Pragmatic Modules>.

=cut

my %bitmask = (
taint	=> 0x00100000,
eval	=> 0x00200000,
);

sub setcolor {
 eval {				# Ignore errors
  require Term::Cap;

  my $terminal = Tgetent Term::Cap ({OSPEED => 9600}); # Avoid warning.
  my $props = $ENV{PERL_RE_TC} || 'md,me,so,se'; # can use us/ue later
  my @props = split /,/, $props;


  $ENV{TERMCAP_COLORS} = join "\t", map {$terminal->Tputs($_,1)} @props;
 };

 not defined $ENV{TERMCAP_COLORS} or ($ENV{TERMCAP_COLORS} =~ tr/\t/\t/) >= 4
    or not defined $ENV{PERL_RE_TC}
    or die "Not enough fields in \$ENV{PERL_RE_TC}=`$ENV{PERL_RE_TC}'";
}

sub bits {
    my $on = shift;
    my $bits = 0;
    unless(@_) {
	require Carp;
	Carp::carp("Useless use of \"re\" pragma");
    }
    foreach my $s (@_){
      if ($s eq 'debug' or $s eq 'debugcolor') {
 	  setcolor() if $s eq 'debugcolor';
	  require DynaLoader;
	  @ISA = ('DynaLoader');
	  bootstrap re;
	  install() if $on;
	  uninstall() unless $on;
	  next;
      }
      $bits |= $bitmask{$s} || 0;
    }
    $bits;
}

sub import {
    shift;
    $^H |= bits(1,@_);
}

sub unimport {
    shift;
    $^H &= ~ bits(0,@_);
}

1;
