package autouse;

#use strict;		# debugging only
use 5.003_90;		# ->can, for my $var

$autouse::VERSION = '1.01';

$autouse::DEBUG ||= 0;

sub vet_import ($);

sub croak {
    require Carp;
    Carp::croak(@_);
}

sub import {
    my $class = @_ ? shift : 'autouse';
    croak "usage: use $class MODULE [,SUBS...]" unless @_;
    my $module = shift;

    (my $pm = $module) =~ s{::}{/}g;
    $pm .= '.pm';
    if (exists $INC{$pm}) {
	vet_import $module;
	local $Exporter::ExportLevel = $Exporter::ExportLevel + 1;
	# $Exporter::Verbose = 1;
	return $module->import(map { (my $f = $_) =~ s/\(.*?\)$// } @_);
    }

    # It is not loaded: need to do real work.
    my $callpkg = caller(0);
    print "autouse called from $callpkg\n" if $autouse::DEBUG;

    my $index;
    for my $f (@_) {
	my $proto;
	$proto = $1 if (my $func = $f) =~ s/\((.*)\)$//;

	my $closure_import_func = $func;	# Full name
	my $closure_func = $func;		# Name inside package
	my $index = index($func, '::');
	if ($index == -1) {
	    $closure_import_func = "${callpkg}::$func";
	} else {
	    $closure_func = substr $func, $index + 2;
	    croak "autouse into different package attempted"
		unless substr($func, 0, $index) eq $module;
	}

	my $load_sub = sub {
	    unless ($INC{$pm}) {
		eval {require $pm};
		die if $@;
		vet_import $module;
	    }
	    *$closure_import_func = \&{"${module}::$closure_func"};
	    print "autousing $module; "
		  ."imported $closure_func as $closure_import_func\n"
		if $autouse::DEBUG;
	    goto &$closure_import_func;
	};

	if (defined $proto) {
	    *$closure_import_func = eval "sub ($proto) { &\$load_sub }";
	} else {
	    *$closure_import_func = $load_sub;
	}
    }
}

sub vet_import ($) {
    my $module = shift;
    if (my $import = $module->can('import')) {
	croak "autoused module has unique import() method"
	    unless defined(&Exporter::import)
		   && $import == \&Exporter::import;
    }
}

1;

__END__

=head1 NAME

autouse - postpone load of modules until a function is used

=head1 SYNOPSIS

  use autouse 'Carp' => qw(carp croak);
  carp "this carp was predeclared and autoused ";

=head1 DESCRIPTION

If the module C<Module> is already loaded, then the declaration

  use autouse 'Module' => qw(func1 func2($;$) Module::func3);

is equivalent to

  use Module qw(func1 func2);

if C<Module> defines func2() with prototype C<($;$)>, and func1() and
func3() have no prototypes.  (At least if C<Module> uses C<Exporter>'s
C<import>, otherwise it is a fatal error.)

If the module C<Module> is not loaded yet, then the above declaration
declares functions func1() and func2() in the current package, and
declares a function Module::func3().  When these functions are called,
they load the package C<Module> if needed, and substitute themselves
with the correct definitions.

=head1 WARNING

Using C<autouse> will move important steps of your program's execution
from compile time to runtime.  This can

=over

=item *

Break the execution of your program if the module you C<autouse>d has
some initialization which it expects to be done early.

=item *

hide bugs in your code since important checks (like correctness of
prototypes) is moved from compile time to runtime.  In particular, if
the prototype you specified on C<autouse> line is wrong, you will not
find it out until the corresponding function is executed.  This will be
very unfortunate for functions which are not always called (note that
for such functions C<autouse>ing gives biggest win, for a workaround
see below).

=back

To alleviate the second problem (partially) it is advised to write
your scripts like this:

  use Module;
  use autouse Module => qw(carp($) croak(&$));
  carp "this carp was predeclared and autoused ";

The first line ensures that the errors in your argument specification
are found early.  When you ship your application you should comment
out the first line, since it makes the second one useless.

=head1 AUTHOR

Ilya Zakharevich (ilya@math.ohio-state.edu)

=head1 SEE ALSO

perl(1).

=cut
