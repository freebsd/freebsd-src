package B::Terse;
use strict;
use B qw(peekop class walkoptree_slow walkoptree_exec
	 main_start main_root cstring svref_2object);
use B::Asmdata qw(@specialsv_name);

sub terse {
    my ($order, $cvref) = @_;
    my $cv = svref_2object($cvref);
    if ($order eq "exec") {
	walkoptree_exec($cv->START, "terse");
    } else {
	walkoptree_slow($cv->ROOT, "terse");
    }
}

sub compile {
    my $order = shift;
    my @options = @_;
    if (@options) {
	return sub {
	    my $objname;
	    foreach $objname (@options) {
		$objname = "main::$objname" unless $objname =~ /::/;
		eval "terse(\$order, \\&$objname)";
		die "terse($order, \\&$objname) failed: $@" if $@;
	    }
	}
    } else {
	if ($order eq "exec") {
	    return sub { walkoptree_exec(main_start, "terse") }
	} else {
	    return sub { walkoptree_slow(main_root, "terse") }
	}
    }
}

sub indent {
    my $level = shift;
    return "    " x $level;
}

sub B::OP::terse {
    my ($op, $level) = @_;
    my $targ = $op->targ;
    $targ = ($targ > 0) ? " [$targ]" : "";
    print indent($level), peekop($op), $targ, "\n";
}

sub B::SVOP::terse {
    my ($op, $level) = @_;
    print indent($level), peekop($op), "  ";
    $op->sv->terse(0);
}

sub B::GVOP::terse {
    my ($op, $level) = @_;
    print indent($level), peekop($op), "  ";
    $op->gv->terse(0);
}

sub B::PMOP::terse {
    my ($op, $level) = @_;
    my $precomp = $op->precomp;
    print indent($level), peekop($op),
	defined($precomp) ? " /$precomp/\n" : " (regexp not compiled)\n";

}

sub B::PVOP::terse {
    my ($op, $level) = @_;
    print indent($level), peekop($op), " ", cstring($op->pv), "\n";
}

sub B::COP::terse {
    my ($op, $level) = @_;
    my $label = $op->label;
    if ($label) {
	$label = " label ".cstring($label);
    }
    print indent($level), peekop($op), $label, "\n";
}

sub B::PV::terse {
    my ($sv, $level) = @_;
    print indent($level);
    printf "%s (0x%lx) %s\n", class($sv), $$sv, cstring($sv->PV);
}

sub B::AV::terse {
    my ($sv, $level) = @_;
    print indent($level);
    printf "%s (0x%lx) FILL %d\n", class($sv), $$sv, $sv->FILL;
}

sub B::GV::terse {
    my ($gv, $level) = @_;
    my $stash = $gv->STASH->NAME;
    if ($stash eq "main") {
	$stash = "";
    } else {
	$stash = $stash . "::";
    }
    print indent($level);
    printf "%s (0x%lx) *%s%s\n", class($gv), $$gv, $stash, $gv->NAME;
}

sub B::IV::terse {
    my ($sv, $level) = @_;
    print indent($level);
    printf "%s (0x%lx) %d\n", class($sv), $$sv, $sv->IV;
}

sub B::NV::terse {
    my ($sv, $level) = @_;
    print indent($level);
    printf "%s (0x%lx) %s\n", class($sv), $$sv, $sv->NV;
}

sub B::NULL::terse {
    my ($sv, $level) = @_;
    print indent($level);
    printf "%s (0x%lx)\n", class($sv), $$sv;
}
    
sub B::SPECIAL::terse {
    my ($sv, $level) = @_;
    print indent($level);
    printf "%s #%d %s\n", class($sv), $$sv, $specialsv_name[$$sv];
}

1;

__END__

=head1 NAME

B::Terse - Walk Perl syntax tree, printing terse info about ops

=head1 SYNOPSIS

	perl -MO=Terse[,OPTIONS] foo.pl

=head1 DESCRIPTION

See F<ext/B/README>.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
