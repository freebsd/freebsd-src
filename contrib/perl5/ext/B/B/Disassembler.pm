#      Disassembler.pm
#
#      Copyright (c) 1996 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
package B::Disassembler::BytecodeStream;
use FileHandle;
use Carp;
use B qw(cstring cast_I32);
@ISA = qw(FileHandle);
sub readn {
    my ($fh, $len) = @_;
    my $data;
    read($fh, $data, $len);
    croak "reached EOF while reading $len bytes" unless length($data) == $len;
    return $data;
}

sub GET_U8 {
    my $fh = shift;
    my $c = $fh->getc;
    croak "reached EOF while reading U8" unless defined($c);
    return ord($c);
}

sub GET_U16 {
    my $fh = shift;
    my $str = $fh->readn(2);
    croak "reached EOF while reading U16" unless length($str) == 2;
    return unpack("n", $str);
}

sub GET_U32 {
    my $fh = shift;
    my $str = $fh->readn(4);
    croak "reached EOF while reading U32" unless length($str) == 4;
    return unpack("N", $str);
}

sub GET_I32 {
    my $fh = shift;
    my $str = $fh->readn(4);
    croak "reached EOF while reading I32" unless length($str) == 4;
    return cast_I32(unpack("N", $str));
}

sub GET_objindex { 
    my $fh = shift;
    my $str = $fh->readn(4);
    croak "reached EOF while reading objindex" unless length($str) == 4;
    return unpack("N", $str);
}

sub GET_strconst {
    my $fh = shift;
    my ($str, $c);
    while (defined($c = $fh->getc) && $c ne "\0") {
	$str .= $c;
    }
    croak "reached EOF while reading strconst" unless defined($c);
    return cstring($str);
}

sub GET_pvcontents {}

sub GET_PV {
    my $fh = shift;
    my $str;
    my $len = $fh->GET_U32;
    if ($len) {
	read($fh, $str, $len);
	croak "reached EOF while reading PV" unless length($str) == $len;
	return cstring($str);
    } else {
	return '""';
    }
}

sub GET_comment_t {
    my $fh = shift;
    my ($str, $c);
    while (defined($c = $fh->getc) && $c ne "\n") {
	$str .= $c;
    }
    croak "reached EOF while reading comment" unless defined($c);
    return cstring($str);
}

sub GET_double {
    my $fh = shift;
    my ($str, $c);
    while (defined($c = $fh->getc) && $c ne "\0") {
	$str .= $c;
    }
    croak "reached EOF while reading double" unless defined($c);
    return $str;
}

sub GET_none {}

sub GET_op_tr_array {
    my $fh = shift;
    my @ary = unpack("n256", $fh->readn(256 * 2));
    return join(",", @ary);
}

sub GET_IV64 {
    my $fh = shift;
    my ($hi, $lo) = unpack("NN", $fh->readn(8));
    return sprintf("0x%4x%04x", $hi, $lo); # cheat
}

package B::Disassembler;
use Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(disassemble_fh);
use Carp;
use strict;

use B::Asmdata qw(%insn_data @insn_name);

sub disassemble_fh {
    my ($fh, $out) = @_;
    my ($c, $getmeth, $insn, $arg);
    bless $fh, "B::Disassembler::BytecodeStream";
    while (defined($c = $fh->getc)) {
	$c = ord($c);
	$insn = $insn_name[$c];
	if (!defined($insn) || $insn eq "unused") {
	    my $pos = $fh->tell - 1;
	    die "Illegal instruction code $c at stream offset $pos\n";
	}
	$getmeth = $insn_data{$insn}->[2];
	$arg = $fh->$getmeth();
	if (defined($arg)) {
	    &$out($insn, $arg);
	} else {
	    &$out($insn);
	}
    }
}

1;

__END__

=head1 NAME

B::Disassembler - Disassemble Perl bytecode

=head1 SYNOPSIS

	use Disassembler;

=head1 DESCRIPTION

See F<ext/B/B/Disassembler.pm>.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
