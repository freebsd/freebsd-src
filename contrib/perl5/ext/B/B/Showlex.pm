package B::Showlex;
use strict;
use B qw(svref_2object comppadlist class);
use B::Terse ();

#
# Invoke as
#     perl -MO=Showlex,foo bar.pl
# to see the names of lexical variables used by &foo
# or as
#     perl -MO=Showlex bar.pl
# to see the names of file scope lexicals used by bar.pl
#    

sub shownamearray {
    my ($name, $av) = @_;
    my @els = $av->ARRAY;
    my $count = @els;
    my $i;
    print "$name has $count entries\n";
    for ($i = 0; $i < $count; $i++) {
        print "$i: ";
	my $sv = $els[$i];
	if (class($sv) ne "SPECIAL") {
	    printf "%s (0x%lx) %s\n", class($sv), $$sv, $sv->PVX;
	} else {
            $sv->terse;
	}
    }
}

sub showvaluearray {
    my ($name, $av) = @_;
    my @els = $av->ARRAY;
    my $count = @els;
    my $i;
    print "$name has $count entries\n";
    for ($i = 0; $i < $count; $i++) {
	print "$i: ";
	$els[$i]->terse;
    }
}

sub showlex {
    my ($objname, $namesav, $valsav) = @_;
    shownamearray("Pad of lexical names for $objname", $namesav);
    showvaluearray("Pad of lexical values for $objname", $valsav);
}

sub showlex_obj {
    my ($objname, $obj) = @_;
    $objname =~ s/^&main::/&/;
    showlex($objname, svref_2object($obj)->PADLIST->ARRAY);
}

sub showlex_main {
    showlex("comppadlist", comppadlist->ARRAY);
}

sub compile {
    my @options = @_;
    if (@options) {
	return sub {
	    my $objname;
	    foreach $objname (@options) {
		$objname = "main::$objname" unless $objname =~ /::/;
		eval "showlex_obj('&$objname', \\&$objname)";
	    }
	}
    } else {
	return \&showlex_main;
    }
}

1;

__END__

=head1 NAME

B::Showlex - Show lexical variables used in functions or files

=head1 SYNOPSIS

	perl -MO=Showlex[,SUBROUTINE] foo.pl

=head1 DESCRIPTION

When a subroutine name is provided in OPTIONS, prints the lexical
variables used in that subroutine.  Otherwise, prints the file-scope
lexicals in the file.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
