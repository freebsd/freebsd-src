#      Assembler.pm
#
#      Copyright (c) 1996 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.

package B::Assembler;
use Exporter;
use B qw(ppname);
use B::Asmdata qw(%insn_data @insn_name);
use Config qw(%Config);
require ByteLoader;		# we just need its $VERSIOM

@ISA = qw(Exporter);
@EXPORT_OK = qw(assemble_fh newasm endasm assemble);
$VERSION = 0.02;

use strict;
my %opnumber;
my ($i, $opname);
for ($i = 0; defined($opname = ppname($i)); $i++) {
    $opnumber{$opname} = $i;
}

my($linenum, $errors, $out); #	global state, set up by newasm

sub error {
    my $str = shift;
    warn "$linenum: $str\n";
    $errors++;
}

my $debug = 0;
sub debug { $debug = shift }

#
# First define all the data conversion subs to which Asmdata will refer
#

sub B::Asmdata::PUT_U8 {
    my $arg = shift;
    my $c = uncstring($arg);
    if (defined($c)) {
	if (length($c) != 1) {
	    error "argument for U8 is too long: $c";
	    $c = substr($c, 0, 1);
	}
    } else {
	$c = chr($arg);
    }
    return $c;
}

sub B::Asmdata::PUT_U16 { pack("S", $_[0]) }
sub B::Asmdata::PUT_U32 { pack("L", $_[0]) }
sub B::Asmdata::PUT_I32 { pack("L", $_[0]) }
sub B::Asmdata::PUT_NV  { sprintf("%s\0", $_[0]) } # "%lf" looses precision and pack('d',...)
						   # may not even be portable between compilers
sub B::Asmdata::PUT_objindex { pack("L", $_[0]) } # could allow names here
sub B::Asmdata::PUT_svindex { &B::Asmdata::PUT_objindex }
sub B::Asmdata::PUT_opindex { &B::Asmdata::PUT_objindex }
sub B::Asmdata::PUT_pvindex { &B::Asmdata::PUT_objindex }

sub B::Asmdata::PUT_strconst {
    my $arg = shift;
    $arg = uncstring($arg);
    if (!defined($arg)) {
	error "bad string constant: $arg";
	return "";
    }
    if ($arg =~ s/\0//g) {
	error "string constant argument contains NUL: $arg";
    }
    return $arg . "\0";
}

sub B::Asmdata::PUT_pvcontents {
    my $arg = shift;
    error "extraneous argument: $arg" if defined $arg;
    return "";
}
sub B::Asmdata::PUT_PV {
    my $arg = shift;
    $arg = uncstring($arg);
    error "bad string argument: $arg" unless defined($arg);
    return pack("L", length($arg)) . $arg;
}
sub B::Asmdata::PUT_comment_t {
    my $arg = shift;
    $arg = uncstring($arg);
    error "bad string argument: $arg" unless defined($arg);
    if ($arg =~ s/\n//g) {
	error "comment argument contains linefeed: $arg";
    }
    return $arg . "\n";
}
sub B::Asmdata::PUT_double { sprintf("%s\0", $_[0]) } # see PUT_NV above
sub B::Asmdata::PUT_none {
    my $arg = shift;
    error "extraneous argument: $arg" if defined $arg;
    return "";
}
sub B::Asmdata::PUT_op_tr_array {
    my $arg = shift;
    my @ary = split(/\s*,\s*/, $arg);
    if (@ary != 256) {
	error "wrong number of arguments to op_tr_array";
	@ary = (0) x 256;
    }
    return pack("S256", @ary);
}
# XXX Check this works
sub B::Asmdata::PUT_IV64 {
    my $arg = shift;
    return pack("LL", $arg >> 32, $arg & 0xffffffff);
}

my %unesc = (n => "\n", r => "\r", t => "\t", a => "\a",
	     b => "\b", f => "\f", v => "\013");

sub uncstring {
    my $s = shift;
    $s =~ s/^"// and $s =~ s/"$// or return undef;
    $s =~ s/\\(\d\d\d|.)/length($1) == 3 ? chr(oct($1)) : ($unesc{$1}||$1)/eg;
    return $s;
}

sub strip_comments {
    my $stmt = shift;
    # Comments only allowed in instructions which don't take string arguments
    $stmt =~ s{
	(?sx)	# Snazzy extended regexp coming up. Also, treat
		# string as a single line so .* eats \n characters.
	^\s*	# Ignore leading whitespace
	(
	  [^"]*	# A double quote '"' indicates a string argument. If we
		# find a double quote, the match fails and we strip nothing.
	)
	\s*\#	# Any amount of whitespace plus the comment marker...
	.*$	# ...which carries on to end-of-string.
    }{$1};	# Keep only the instruction and optional argument.
    return $stmt;
}

# create the ByteCode header: magic, archname, ByteLoader $VERSION, ivsize,
# 	ptrsize, byteorder
# nvtype is irrelevant (floats are stored as strings)
# byteorder is strconst not U32 because of varying size issues

sub gen_header {
    my $header = "";

    $header .= B::Asmdata::PUT_U32(0x43424c50);	# 'PLBC'
    $header .= B::Asmdata::PUT_strconst('"' . $Config{archname}. '"');
    $header .= B::Asmdata::PUT_strconst(qq["$ByteLoader::VERSION"]);
    $header .= B::Asmdata::PUT_U32($Config{ivsize});
    $header .= B::Asmdata::PUT_U32($Config{ptrsize});
    $header .= B::Asmdata::PUT_strconst(sprintf(qq["0x%s"], $Config{byteorder}));

    $header;
}

sub parse_statement {
    my $stmt = shift;
    my ($insn, $arg) = $stmt =~ m{
	(?sx)
	^\s*	# allow (but ignore) leading whitespace
	(.*?)	# Instruction continues up until...
	(?:	# ...an optional whitespace+argument group
	    \s+		# first whitespace.
	    (.*)	# The argument is all the rest (newlines included).
	)?$	# anchor at end-of-line
    };	
    if (defined($arg)) {
	if ($arg =~ s/^0x(?=[0-9a-fA-F]+$)//) {
	    $arg = hex($arg);
	} elsif ($arg =~ s/^0(?=[0-7]+$)//) {
	    $arg = oct($arg);
	} elsif ($arg =~ /^pp_/) {
	    $arg =~ s/\s*$//; # strip trailing whitespace
	    my $opnum = $opnumber{$arg};
	    if (defined($opnum)) {
		$arg = $opnum;
	    } else {
		error qq(No such op type "$arg");
		$arg = 0;
	    }
	}
    }
    return ($insn, $arg);
}

sub assemble_insn {
    my ($insn, $arg) = @_;
    my $data = $insn_data{$insn};
    if (defined($data)) {
	my ($bytecode, $putsub) = @{$data}[0, 1];
	my $argcode = &$putsub($arg);
	return chr($bytecode).$argcode;
    } else {
	error qq(no such instruction "$insn");
	return "";
    }
}

sub assemble_fh {
    my ($fh, $out) = @_;
    my $line;
    my $asm = newasm($out);
    while ($line = <$fh>) {
	assemble($line);
    }
    endasm();
}

sub newasm {
    my($outsub) = @_;

    die "Invalid printing routine for B::Assembler\n" unless ref $outsub eq 'CODE';
    die <<EOD if ref $out;
Can't have multiple byteassembly sessions at once!
	(perhaps you forgot an endasm()?)
EOD

    $linenum = $errors = 0;
    $out = $outsub;

    $out->(gen_header());
}

sub endasm {
    if ($errors) {
	die "There were $errors assembly errors\n";
    }
    $linenum = $errors = $out = 0;
}

sub assemble {
    my($line) = @_;
    my ($insn, $arg);
    $linenum++;
    chomp $line;
    if ($debug) {
	my $quotedline = $line;
	$quotedline =~ s/\\/\\\\/g;
	$quotedline =~ s/"/\\"/g;
	$out->(assemble_insn("comment", qq("$quotedline")));
    }
    $line = strip_comments($line) or next;
    ($insn, $arg) = parse_statement($line);
    $out->(assemble_insn($insn, $arg));
    if ($debug) {
	$out->(assemble_insn("nop", undef));
    }
}

1;

__END__

=head1 NAME

B::Assembler - Assemble Perl bytecode

=head1 SYNOPSIS

	use B::Assembler qw(newasm endasm assemble);
	newasm(\&printsub);	# sets up for assembly
	assemble($buf); 	# assembles one line
	endasm();		# closes down

	use B::Assembler qw(assemble_fh);
	assemble_fh($fh, \&printsub);	# assemble everything in $fh

=head1 DESCRIPTION

See F<ext/B/B/Assembler.pm>.

=head1 AUTHORS

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>
Per-statement interface by Benjamin Stuhl, C<sho_pi@hotmail.com>

=cut
