package O;
use B qw(minus_c save_BEGINs);
use Carp;    

sub import {
    my ($class, $backend, @options) = @_;
    eval "use B::$backend ()";
    if ($@) {
	croak "use of backend $backend failed: $@";
    }
    my $compilesub = &{"B::${backend}::compile"}(@options);
    if (ref($compilesub) eq "CODE") {
	minus_c;
	save_BEGINs;
	eval 'CHECK { &$compilesub() }';
    } else {
	die $compilesub;
    }
}

1;

__END__

=head1 NAME

O - Generic interface to Perl Compiler backends

=head1 SYNOPSIS

	perl -MO=Backend[,OPTIONS] foo.pl

=head1 DESCRIPTION

This is the module that is used as a frontend to the Perl Compiler.

=head1 CONVENTIONS

Most compiler backends use the following conventions: OPTIONS
consists of a comma-separated list of words (no white-space).
The C<-v> option usually puts the backend into verbose mode.
The C<-ofile> option generates output to B<file> instead of
stdout. The C<-D> option followed by various letters turns on
various internal debugging flags. See the documentation for the
desired backend (named C<B::Backend> for the example above) to
find out about that backend.

=head1 IMPLEMENTATION

This section is only necessary for those who want to write a
compiler backend module that can be used via this module.

The command-line mentioned in the SYNOPSIS section corresponds to
the Perl code

    use O ("Backend", OPTIONS);

The C<import> function which that calls loads in the appropriate
C<B::Backend> module and calls the C<compile> function in that
package, passing it OPTIONS. That function is expected to return
a sub reference which we'll call CALLBACK. Next, the "compile-only"
flag is switched on (equivalent to the command-line option C<-c>)
and a CHECK block is registered which calls CALLBACK. Thus the main
Perl program mentioned on the command-line is read in, parsed and
compiled into internal syntax tree form. Since the C<-c> flag is
set, the program does not start running (excepting BEGIN blocks of
course) but the CALLBACK function registered by the compiler
backend is called.

In summary, a compiler backend module should be called "B::Foo"
for some foo and live in the appropriate directory for that name.
It should define a function called C<compile>. When the user types

    perl -MO=Foo,OPTIONS foo.pl

that function is called and is passed those OPTIONS (split on
commas). It should return a sub ref to the main compilation function.
After the user's program is loaded and parsed, that returned sub ref
is invoked which can then go ahead and do the compilation, usually by
making use of the C<B> module's functionality.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
