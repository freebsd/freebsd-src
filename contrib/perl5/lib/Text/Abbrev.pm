package Text::Abbrev;
require 5.000;
require Exporter;

=head1 NAME

abbrev - create an abbreviation table from a list

=head1 SYNOPSIS

    use Text::Abbrev;
    abbrev $hashref, LIST


=head1 DESCRIPTION

Stores all unambiguous truncations of each element of LIST
as keys key in the associative array referenced to by C<$hashref>.
The values are the original list elements.

=head1 EXAMPLE

    $hashref = abbrev qw(list edit send abort gripe);

    %hash = abbrev qw(list edit send abort gripe);

    abbrev $hashref, qw(list edit send abort gripe);

    abbrev(*hash, qw(list edit send abort gripe));

=cut

@ISA = qw(Exporter);
@EXPORT = qw(abbrev);

# Usage:
#	&abbrev(*foo,LIST);
#	...
#	$long = $foo{$short};

sub abbrev {
    my (%domain);
    my ($name, $ref, $glob);

    if (ref($_[0])) {           # hash reference preferably
      $ref = shift;
    } elsif ($_[0] =~ /^\*/) {  # looks like a glob (deprecated)
      $glob = shift;
    } 
    my @cmp = @_;

    foreach $name (@_) {
	my @extra = split(//,$name);
	my $abbrev = shift(@extra);
	my $len = 1;
        my $cmp;
	WORD: foreach $cmp (@cmp) {
	    next if $cmp eq $name;
	    while (substr($cmp,0,$len) eq $abbrev) {
                last WORD unless @extra;
                $abbrev .= shift(@extra);
		++$len;
	    }
	}
	$domain{$abbrev} = $name;
	while (@extra) {
	    $abbrev .= shift(@extra);
	    $domain{$abbrev} = $name;
	}
    }
    if ($ref) {
      %$ref = %domain;
      return;
    } elsif ($glob) {           # old style
      local (*hash) = $glob;
      %hash = %domain;
      return;
    }
    if (wantarray) {
      %domain;
    } else {
      \%domain;
    }
}

1;

