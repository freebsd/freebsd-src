package Env;

=head1 NAME

Env - perl module that imports environment variables

=head1 SYNOPSIS

    use Env;
    use Env qw(PATH HOME TERM);

=head1 DESCRIPTION

Perl maintains environment variables in a pseudo-hash named %ENV.  For
when this access method is inconvenient, the Perl module C<Env> allows
environment variables to be treated as simple variables.

The Env::import() function ties environment variables with suitable
names to global Perl variables with the same names.  By default it
does so with all existing environment variables (C<keys %ENV>).  If
the import function receives arguments, it takes them to be a list of
environment variables to tie; it's okay if they don't yet exist.

After an environment variable is tied, merely use it like a normal variable.
You may access its value 

    @path = split(/:/, $PATH);

or modify it

    $PATH .= ":.";

however you'd like.
To remove a tied environment variable from
the environment, assign it the undefined value

    undef $PATH;

=head1 AUTHOR

Chip Salzenberg E<lt>F<chip@fin.uucp>E<gt>

=cut

sub import {
    my ($callpack) = caller(0);
    my $pack = shift;
    my @vars = grep /^[A-Za-z_]\w*$/, (@_ ? @_ : keys(%ENV));
    return unless @vars;

    eval "package $callpack; use vars qw("
	 . join(' ', map { '$'.$_ } @vars) . ")";
    die $@ if $@;
    foreach (@vars) {
	tie ${"${callpack}::$_"}, Env, $_;
    }
}

sub TIESCALAR {
    bless \($_[1]);
}

sub FETCH {
    my ($self) = @_;
    $ENV{$$self};
}

sub STORE {
    my ($self, $value) = @_;
    if (defined($value)) {
	$ENV{$$self} = $value;
    } else {
	delete $ENV{$$self};
    }
}

1;
