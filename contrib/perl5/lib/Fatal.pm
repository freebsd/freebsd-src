package Fatal;

use Carp;
use strict;
use vars qw( $AUTOLOAD $Debug $VERSION);

$VERSION = 1.02;

$Debug = 0 unless defined $Debug;

sub import {
    my $self = shift(@_);
    my($sym, $pkg);
    $pkg = (caller)[0];
    foreach $sym (@_) {
	&_make_fatal($sym, $pkg);
    }
};

sub AUTOLOAD {
    my $cmd = $AUTOLOAD;
    $cmd =~ s/.*:://;
    &_make_fatal($cmd, (caller)[0]);
    goto &$AUTOLOAD;
}

sub fill_protos {
  my $proto = shift;
  my ($n, $isref, @out, @out1, $seen_semi) = -1;
  while ($proto =~ /\S/) {
    $n++;
    push(@out1,[$n,@out]) if $seen_semi;
    push(@out, $1 . "{\$_[$n]}"), next if $proto =~ s/^\s*\\([\@%\$\&])//;
    push(@out, "\$_[$n]"), next if $proto =~ s/^\s*([*\$&])//;
    push(@out, "\@_[$n..\$#_]"), last if $proto =~ s/^\s*(;\s*)?\@//;
    $seen_semi = 1, $n--, next if $proto =~ s/^\s*;//; # XXXX ????
    die "Unknown prototype letters: \"$proto\"";
  }
  push(@out1,[$n+1,@out]);
  @out1;
}

sub write_invocation {
  my ($core, $call, $name, @argvs) = @_;
  if (@argvs == 1) {		# No optional arguments
    my @argv = @{$argvs[0]};
    shift @argv;
    return "\t" . one_invocation($core, $call, $name, @argv) . ";\n";
  } else {
    my $else = "\t";
    my (@out, @argv, $n);
    while (@argvs) {
      @argv = @{shift @argvs};
      $n = shift @argv;
      push @out, "$ {else}if (\@_ == $n) {\n";
      $else = "\t} els";
      push @out, 
          "\t\treturn " . one_invocation($core, $call, $name, @argv) . ";\n";
    }
    push @out, <<EOC;
	}
	die "$name(\@_): Do not expect to get ", scalar \@_, " arguments";
EOC
    return join '', @out;
  }
}

sub one_invocation {
  my ($core, $call, $name, @argv) = @_;
  local $" = ', ';
  return qq{$call(@argv) || croak "Can't $name(\@_)} . 
    ($core ? ': $!' : ', \$! is \"$!\"') . '"';
}

sub _make_fatal {
    my($sub, $pkg) = @_;
    my($name, $code, $sref, $real_proto, $proto, $core, $call);
    my $ini = $sub;

    $sub = "${pkg}::$sub" unless $sub =~ /::/;
    $name = $sub;
    $name =~ s/.*::// or $name =~ s/^&//;
    print "# _make_fatal: sub=$sub pkg=$pkg name=$name\n" if $Debug;
    croak "Bad subroutine name for Fatal: $name" unless $name =~ /^\w+$/;
    if (defined(&$sub)) {	# user subroutine
	$sref = \&$sub;
	$proto = prototype $sref;
	$call = '&$sref';
    } elsif ($sub eq $ini) {	# Stray user subroutine
	die "$sub is not a Perl subroutine" 
    } else {			# CORE subroutine
        $proto = eval { prototype "CORE::$name" };
	die "$name is neither a builtin, nor a Perl subroutine" 
	  if $@;
	die "Cannot make a non-overridable builtin fatal"
	  if not defined $proto;
	$core = 1;
	$call = "CORE::$name";
    }
    if (defined $proto) {
      $real_proto = " ($proto)";
    } else {
      $real_proto = '';
      $proto = '@';
    }
    $code = <<EOS;
sub$real_proto {
	local(\$", \$!) = (', ', 0);
EOS
    my @protos = fill_protos($proto);
    $code .= write_invocation($core, $call, $name, @protos);
    $code .= "}\n";
    print $code if $Debug;
    {
      no strict 'refs'; # to avoid: Can't use string (...) as a symbol ref ...
      $code = eval("package $pkg; use Carp; $code");
      die if $@;
      local($^W) = 0;   # to avoid: Subroutine foo redefined ...
      *{$sub} = $code;
    }
}

1;

__END__

=head1 NAME

Fatal - replace functions with equivalents which succeed or die

=head1 SYNOPSIS

    use Fatal qw(open close);

    sub juggle { . . . }
    import Fatal 'juggle';

=head1 DESCRIPTION

C<Fatal> provides a way to conveniently replace functions which normally
return a false value when they fail with equivalents which halt execution
if they are not successful.  This lets you use these functions without
having to test their return values explicitly on each call.   Errors are
reported via C<die>, so you can trap them using C<$SIG{__DIE__}> if you
wish to take some action before the program exits.

The do-or-die equivalents are set up simply by calling Fatal's
C<import> routine, passing it the names of the functions to be
replaced.  You may wrap both user-defined functions and overridable
CORE operators (except C<exec>, C<system> which cannot be expressed
via prototypes) in this way.

=head1 AUTHOR

Lionel.Cons@cern.ch

prototype updates by Ilya Zakharevich ilya@math.ohio-state.edu

=cut
