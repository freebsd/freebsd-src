use Config;

sub to_string {
    my ($value) = @_;
    $value =~ s/\\/\\\\/g;
    $value =~ s/'/\\'/g;
    return "'$value'";
}

unlink "XSLoader.pm" if -f "XSLoader.pm";
open OUT, ">XSLoader.pm" or die $!;
print OUT <<'EOT';
# Generated from XSLoader.pm.PL (resolved %Config::Config value)

package XSLoader;

#   And Gandalf said: 'Many folk like to know beforehand what is to
#   be set on the table; but those who have laboured to prepare the
#   feast like to keep their secret; for wonder makes the words of
#   praise louder.'

#   (Quote from Tolkien sugested by Anno Siegel.)
#
# See pod text at end of file for documentation.
# See also ext/DynaLoader/README in source tree for other information.
#
# Tim.Bunce@ig.co.uk, August 1994

$VERSION = "0.01";	# avoid typo warning

# enable debug/trace messages from DynaLoader perl code
# $dl_debug = $ENV{PERL_DL_DEBUG} || 0 unless defined $dl_debug;

EOT

print OUT '  my $dl_dlext = ', to_string($Config::Config{'dlext'}), ";\n" ;

print OUT <<'EOT';

package DynaLoader;

# No prizes for guessing why we don't say 'bootstrap DynaLoader;' here.
# NOTE: All dl_*.xs (including dl_none.xs) define a dl_error() XSUB
boot_DynaLoader('DynaLoader') if defined(&boot_DynaLoader) &&
                                !defined(&dl_error);
package XSLoader;

1; # End of main code

# The bootstrap function cannot be autoloaded (without complications)
# so we define it here:

sub load {
    package DynaLoader;

    my($module) = $_[0];

    # work with static linking too
    my $b = "$module\::bootstrap";
    goto &$b if defined &$b;

    goto retry unless $module and defined &dl_load_file;

    my @modparts = split(/::/,$module);
    my $modfname = $modparts[-1];

EOT

print OUT <<'EOT' if defined &DynaLoader::mod2fname;
    # Some systems have restrictions on files names for DLL's etc.
    # mod2fname returns appropriate file base name (typically truncated)
    # It may also edit @modparts if required.
    $modfname = &mod2fname(\@modparts) if defined &mod2fname;

EOT

print OUT <<'EOT';
    my $modpname = join('/',@modparts);
    my $modlibname = (caller())[1];
    my $c = @modparts;
    $modlibname =~ s,[\\/][^\\/]+$,, while $c--;	# Q&D basename
    my $file = "$modlibname/auto/$modpname/$modfname.$dl_dlext";

#   print STDERR "XSLoader::load for $module ($file)\n" if $dl_debug;

    my $bs = $file;
    $bs =~ s/(\.\w+)?(;\d*)?$/\.bs/; # look for .bs 'beside' the library

    goto retry if not -f $file or -s $bs;

    my $bootname = "boot_$module";
    $bootname =~ s/\W/_/g;
    @dl_require_symbols = ($bootname);

    # Many dynamic extension loading problems will appear to come from
    # this section of code: XYZ failed at line 123 of DynaLoader.pm.
    # Often these errors are actually occurring in the initialisation
    # C code of the extension XS file. Perl reports the error as being
    # in this perl code simply because this was the last perl code
    # it executed.

    my $libref = dl_load_file($file, 0) or do { 
	require Carp;
	Carp::croak("Can't load '$file' for module $module: " . dl_error());
    };
    push(@dl_librefs,$libref);  # record loaded object

    my @unresolved = dl_undef_symbols();
    if (@unresolved) {
	require Carp;
	Carp::carp("Undefined symbols present after loading $file: @unresolved\n");
    }

    my $boot_symbol_ref = dl_find_symbol($libref, $bootname) or do {
	require Carp;
	Carp::croak("Can't find '$bootname' symbol in $file\n");
    };

    my $xs = dl_install_xsub("${module}::bootstrap", $boot_symbol_ref, $file);

    push(@dl_modules, $module); # record loaded module

    # See comment block above
    return &$xs(@_);

  retry:
    require DynaLoader;
    goto &DynaLoader::bootstrap_inherit;
}

__END__

=head1 NAME

XSLoader - Dynamically load C libraries into Perl code

=head1 SYNOPSIS

    package YourPackage;
    use XSLoader;

    XSLoader::load 'YourPackage', @args;

=head1 DESCRIPTION

This module defines a standard I<simplified> interface to the dynamic
linking mechanisms available on many platforms.  Its primary purpose is
to implement cheap automatic dynamic loading of Perl modules.

For more complicated interface see L<DynaLoader>.

=head1 AUTHOR

Ilya Zakharevich: extraction from DynaLoader.

=cut
EOT

close OUT or die $!;

