#      Stackobj.pm
#
#      Copyright (c) 1996 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
#
package B::Stackobj;  
use Exporter ();
@ISA = qw(Exporter);
@EXPORT_OK = qw(set_callback T_UNKNOWN T_DOUBLE T_INT VALID_UNSIGNED
		VALID_INT VALID_DOUBLE VALID_SV REGISTER TEMPORARY);
%EXPORT_TAGS = (types => [qw(T_UNKNOWN T_DOUBLE T_INT)],
		flags => [qw(VALID_INT VALID_DOUBLE VALID_SV
			     VALID_UNSIGNED REGISTER TEMPORARY)]);

use Carp qw(confess);
use strict;
use B qw(class SVf_IOK SVf_NOK SVf_IVisUV);

# Types
sub T_UNKNOWN () { 0 }
sub T_DOUBLE ()  { 1 }
sub T_INT ()     { 2 }
sub T_SPECIAL () { 3 }

# Flags
sub VALID_INT ()	{ 0x01 }
sub VALID_UNSIGNED ()	{ 0x02 }
sub VALID_DOUBLE ()	{ 0x04 }
sub VALID_SV ()		{ 0x08 }
sub REGISTER ()		{ 0x10 } # no implicit write-back when calling subs
sub TEMPORARY ()	{ 0x20 } # no implicit write-back needed at all
sub SAVE_INT () 	{ 0x40 } #if int part needs to be saved at all
sub SAVE_DOUBLE () 	{ 0x80 } #if double part needs to be saved at all


#
# Callback for runtime code generation
#
my $runtime_callback = sub { confess "set_callback not yet called" };
sub set_callback (&) { $runtime_callback = shift }
sub runtime { &$runtime_callback(@_) }

#
# Methods
#

sub write_back { confess "stack object does not implement write_back" }

sub invalidate { shift->{flags} &= ~(VALID_INT |VALID_UNSIGNED | VALID_DOUBLE) }

sub as_sv {
    my $obj = shift;
    if (!($obj->{flags} & VALID_SV)) {
	$obj->write_back;
	$obj->{flags} |= VALID_SV;
    }
    return $obj->{sv};
}

sub as_int {
    my $obj = shift;
    if (!($obj->{flags} & VALID_INT)) {
	$obj->load_int;
	$obj->{flags} |= VALID_INT|SAVE_INT;
    }
    return $obj->{iv};
}

sub as_double {
    my $obj = shift;
    if (!($obj->{flags} & VALID_DOUBLE)) {
	$obj->load_double;
	$obj->{flags} |= VALID_DOUBLE|SAVE_DOUBLE;
    }
    return $obj->{nv};
}

sub as_numeric {
    my $obj = shift;
    return $obj->{type} == T_INT ? $obj->as_int : $obj->as_double;
}

sub as_bool {
	my $obj=shift;
	if ($obj->{flags} & VALID_INT ){
		return $obj->{iv}; 
	}
	if ($obj->{flags} & VALID_DOUBLE ){
		return $obj->{nv}; 
	}
	return sprintf("(SvTRUE(%s))", $obj->as_sv) ;
}

#
# Debugging methods
#
sub peek {
    my $obj = shift;
    my $type = $obj->{type};
    my $flags = $obj->{flags};
    my @flags;
    if ($type == T_UNKNOWN) {
	$type = "T_UNKNOWN";
    } elsif ($type == T_INT) {
	$type = "T_INT";
    } elsif ($type == T_DOUBLE) {
	$type = "T_DOUBLE";
    } else {
	$type = "(illegal type $type)";
    }
    push(@flags, "VALID_INT") if $flags & VALID_INT;
    push(@flags, "VALID_DOUBLE") if $flags & VALID_DOUBLE;
    push(@flags, "VALID_SV") if $flags & VALID_SV;
    push(@flags, "REGISTER") if $flags & REGISTER;
    push(@flags, "TEMPORARY") if $flags & TEMPORARY;
    @flags = ("none") unless @flags;
    return sprintf("%s type=$type flags=%s sv=$obj->{sv}",
		   class($obj), join("|", @flags));
}

sub minipeek {
    my $obj = shift;
    my $type = $obj->{type};
    my $flags = $obj->{flags};
    if ($type == T_INT || $flags & VALID_INT) {
	return $obj->{iv};
    } elsif ($type == T_DOUBLE || $flags & VALID_DOUBLE) {
	return $obj->{nv};
    } else {
	return $obj->{sv};
    }
}

#
# Caller needs to ensure that set_int, set_double,
# set_numeric and set_sv are only invoked on legal lvalues.
#
sub set_int {
    my ($obj, $expr,$unsigned) = @_;
    runtime("$obj->{iv} = $expr;");
    $obj->{flags} &= ~(VALID_SV | VALID_DOUBLE);
    $obj->{flags} |= VALID_INT|SAVE_INT;
    $obj->{flags} |= VALID_UNSIGNED if $unsigned; 
}

sub set_double {
    my ($obj, $expr) = @_;
    runtime("$obj->{nv} = $expr;");
    $obj->{flags} &= ~(VALID_SV | VALID_INT);
    $obj->{flags} |= VALID_DOUBLE|SAVE_DOUBLE;
}

sub set_numeric {
    my ($obj, $expr) = @_;
    if ($obj->{type} == T_INT) {
	$obj->set_int($expr);
    } else {
	$obj->set_double($expr);
    }
}

sub set_sv {
    my ($obj, $expr) = @_;
    runtime("SvSetSV($obj->{sv}, $expr);");
    $obj->invalidate;
    $obj->{flags} |= VALID_SV;
}

#
# Stackobj::Padsv
#

@B::Stackobj::Padsv::ISA = 'B::Stackobj';
sub B::Stackobj::Padsv::new {
    my ($class, $type, $extra_flags, $ix, $iname, $dname) = @_;
    $extra_flags |= SAVE_INT if $extra_flags & VALID_INT;
    $extra_flags |= SAVE_DOUBLE if $extra_flags & VALID_DOUBLE;
    bless {
	type => $type,
	flags => VALID_SV | $extra_flags,
	sv => "PL_curpad[$ix]",
	iv => "$iname",
	nv => "$dname"
    }, $class;
}

sub B::Stackobj::Padsv::load_int {
    my $obj = shift;
    if ($obj->{flags} & VALID_DOUBLE) {
	runtime("$obj->{iv} = $obj->{nv};");
    } else {
	runtime("$obj->{iv} = SvIV($obj->{sv});");
    }
    $obj->{flags} |= VALID_INT|SAVE_INT;
}

sub B::Stackobj::Padsv::load_double {
    my $obj = shift;
    $obj->write_back;
    runtime("$obj->{nv} = SvNV($obj->{sv});");
    $obj->{flags} |= VALID_DOUBLE|SAVE_DOUBLE;
}
sub B::Stackobj::Padsv::save_int {
    my $obj = shift;
    return $obj->{flags} & SAVE_INT;
}

sub B::Stackobj::Padsv::save_double {
    my $obj = shift;
    return $obj->{flags} & SAVE_DOUBLE;
}

sub B::Stackobj::Padsv::write_back {
    my $obj = shift;
    my $flags = $obj->{flags};
    return if $flags & VALID_SV;
    if ($flags & VALID_INT) {
        if ($flags & VALID_UNSIGNED ){
            runtime("sv_setuv($obj->{sv}, $obj->{iv});");
        }else{
            runtime("sv_setiv($obj->{sv}, $obj->{iv});");
        }     
    } elsif ($flags & VALID_DOUBLE) {
	runtime("sv_setnv($obj->{sv}, $obj->{nv});");
    } else {
	confess "write_back failed for lexical @{[$obj->peek]}\n";
    }
    $obj->{flags} |= VALID_SV;
}

#
# Stackobj::Const
#

@B::Stackobj::Const::ISA = 'B::Stackobj';
sub B::Stackobj::Const::new {
    my ($class, $sv) = @_;
    my $obj = bless {
	flags => 0,
	sv => $sv    # holds the SV object until write_back happens
    }, $class;
    if ( ref($sv) eq  "B::SPECIAL" ){
	$obj->{type}= T_SPECIAL;	
    }else{
    	my $svflags = $sv->FLAGS;
    	if ($svflags & SVf_IOK) {
		$obj->{flags} = VALID_INT|VALID_DOUBLE;
		$obj->{type} = T_INT;
                if ($svflags & SVf_IVisUV){
                    $obj->{flags} |= VALID_UNSIGNED;
                    $obj->{nv} = $obj->{iv} = $sv->UVX;
                }else{
                    $obj->{nv} = $obj->{iv} = $sv->IV;
                }
    	} elsif ($svflags & SVf_NOK) {
		$obj->{flags} = VALID_INT|VALID_DOUBLE;
		$obj->{type} = T_DOUBLE;
		$obj->{iv} = $obj->{nv} = $sv->NV;
    	} else {
		$obj->{type} = T_UNKNOWN;
    	}
    }
    return $obj;
}

sub B::Stackobj::Const::write_back {
    my $obj = shift;
    return if $obj->{flags} & VALID_SV;
    # Save the SV object and replace $obj->{sv} by its C source code name
    $obj->{sv} = $obj->{sv}->save;
    $obj->{flags} |= VALID_SV|VALID_INT|VALID_DOUBLE;
}

sub B::Stackobj::Const::load_int {
    my $obj = shift;
    if (ref($obj->{sv}) eq "B::RV"){
       $obj->{iv} = int($obj->{sv}->RV->PV);
    }else{
       $obj->{iv} = int($obj->{sv}->PV);
    }
    $obj->{flags} |= VALID_INT;
}

sub B::Stackobj::Const::load_double {
    my $obj = shift;
    if (ref($obj->{sv}) eq "B::RV"){
        $obj->{nv} = $obj->{sv}->RV->PV + 0.0;
    }else{
        $obj->{nv} = $obj->{sv}->PV + 0.0;
    }
    $obj->{flags} |= VALID_DOUBLE;
}

sub B::Stackobj::Const::invalidate {}

#
# Stackobj::Bool
#

@B::Stackobj::Bool::ISA = 'B::Stackobj';
sub B::Stackobj::Bool::new {
    my ($class, $preg) = @_;
    my $obj = bless {
	type => T_INT,
	flags => VALID_INT|VALID_DOUBLE,
	iv => $$preg,
	nv => $$preg,
	preg => $preg		# this holds our ref to the pseudo-reg
    }, $class;
    return $obj;
}

sub B::Stackobj::Bool::write_back {
    my $obj = shift;
    return if $obj->{flags} & VALID_SV;
    $obj->{sv} = "($obj->{iv} ? &PL_sv_yes : &PL_sv_no)";
    $obj->{flags} |= VALID_SV;
}

# XXX Might want to handle as_double/set_double/load_double?

sub B::Stackobj::Bool::invalidate {}

1;

__END__

=head1 NAME

B::Stackobj - Helper module for CC backend

=head1 SYNOPSIS

	use B::Stackobj;

=head1 DESCRIPTION

See F<ext/B/README>.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
