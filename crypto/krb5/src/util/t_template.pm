package t_template;

use strict;
use vars qw(@ISA @EXPORT_OK);

@ISA=();
@EXPORT_OK= qw(init setparm output);

sub init { # (\@parms, \%defaults, \@template)
    my ($self, $parms, $defs, $templatelines) = @_;
    $self->{parms} = { };
    $self->{values} = { };
    my $key;
    foreach $key (@$parms) {
	$self->{parms}{$key} = 1;
    }
    foreach $key (keys %$defs) {
	$self->{values}{$key} = ${$defs}{$key};
    }
    if (defined($templatelines)) {
	$self->{template} = join "", @$templatelines;
    }
}

sub validateparm { # (parmname)
    no strict 'refs';
    my ($self, $parmname) = @_;
    if (!defined($self->{parms}{$parmname})) {
	die "unknown parameter $parmname";
    }
}

sub setparm { # (parm, value)
    my ($self, $parm, $value) = @_;
    $self->validateparm($parm);
    $self->{values}{$parm} = $value;
}

sub substitute { # (text)
    my ($self, $text) = @_;
    my ($p);

    # Do substitutions.
    foreach $p (keys %{$self->{parms}}) {
	if (!defined $self->{values}{$p}) {
	    die "$0: No value supplied for parameter $p\n";
	}
	# XXX More careful quoting of supplied value!
	$text =~ s|<$p>|$self->{values}{$p}|g;
    }
    return $text;
}

sub output { # (fh)
    my ($self, $fh) = @_;
    print $fh "/* start of ", ref($self), " template */\n";
    print $fh $self->substitute($self->{template});
    print $fh "/* end of ", ref($self), " template */\n";
}

1;
