#      Bytecode.pm
#
#      Copyright (c) 1996-1998 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
#
package B::Bytecode;

use strict;
use Carp;
use B qw(main_cv main_root main_start comppadlist
	 class peekop walkoptree svref_2object cstring walksymtable
	 init_av begin_av end_av
	 SVf_POK SVp_POK SVf_IOK SVp_IOK SVf_NOK SVp_NOK
	 SVf_READONLY GVf_IMPORTED_AV GVf_IMPORTED_CV GVf_IMPORTED_HV
	 GVf_IMPORTED_SV SVTYPEMASK
	);
use B::Asmdata qw(@optype @specialsv_name);
use B::Assembler qw(newasm endasm assemble);

my %optype_enum;
my $i;
for ($i = 0; $i < @optype; $i++) {
    $optype_enum{$optype[$i]} = $i;
}

# Following is SVf_POK|SVp_POK
# XXX Shouldn't be hardwired
sub POK () { SVf_POK|SVp_POK }

# Following is SVf_IOK|SVp_IOK
# XXX Shouldn't be hardwired
sub IOK () { SVf_IOK|SVp_IOK }

# Following is SVf_NOK|SVp_NOK
# XXX Shouldn't be hardwired
sub NOK () { SVf_NOK|SVp_NOK }

# nonexistant flags (see B::GV::bytecode for usage)
sub GVf_IMPORTED_IO () { 0; }
sub GVf_IMPORTED_FORM () { 0; }

my ($verbose, $no_assemble, $debug_bc, $debug_cv);
my @packages;	# list of packages to compile

sub asm (@) {	# print replacement that knows about assembling
    if ($no_assemble) {
	print @_;
    } else {
	my $buf = join '', @_;
	assemble($_) for (split /\n/, $buf);
    }
}

sub asmf (@) {	# printf replacement that knows about assembling
    if ($no_assemble) {
	printf shift(), @_;
    } else {
	my $format = shift;
	my $buf = sprintf $format, @_;
	assemble($_) for (split /\n/, $buf);
    }
}

# Optimisation options. On the command line, use hyphens instead of
# underscores for compatibility with gcc-style options. We use
# underscores here because they are OK in (strict) barewords.
my ($compress_nullops, $omit_seq, $bypass_nullops);
my %optimise = (compress_nullops	=> \$compress_nullops,
		omit_sequence_numbers	=> \$omit_seq,
		bypass_nullops		=> \$bypass_nullops);

my $strip_syntree;	# this is left here in case stripping the
			# syntree ever becomes safe again
			#	-- BKS, June 2000

my $nextix = 0;
my %symtable;	# maps object addresses to object indices.
		# Filled in at allocation (newsv/newop) time.

my %saved;	# maps object addresses (for SVish classes) to "saved yet?"
		# flag. Set at FOO::bytecode time usually by SV::bytecode.
		# Manipulated via saved(), mark_saved(), unmark_saved().

my %strtable;	# maps shared strings to object indices
		# Filled in at allocation (pvix) time

my $svix = -1;	# we keep track of when the sv register contains an element
		# of the object table to avoid unnecessary repeated
		# consecutive ldsv instructions.

my $opix = -1;	# Ditto for the op register.

sub ldsv {
    my $ix = shift;
    if ($ix != $svix) {
	asm "ldsv $ix\n";
	$svix = $ix;
    }
}

sub stsv {
    my $ix = shift;
    asm "stsv $ix\n";
    $svix = $ix;
}

sub set_svix {
    $svix = shift;
}

sub ldop {
    my $ix = shift;
    if ($ix != $opix) {
	asm "ldop $ix\n";
	$opix = $ix;
    }
}

sub stop {
    my $ix = shift;
    asm "stop $ix\n";
    $opix = $ix;
}

sub set_opix {
    $opix = shift;
}

sub pvstring {
    my $str = shift;
    if (defined($str)) {
	return cstring($str . "\0");
    } else {
	return '""';
    }
}

sub nv {
    # print full precision
    my $str = sprintf "%.40f", $_[0];
    $str =~ s/0+$//;		# remove trailing zeros
    $str =~ s/\.$/.0/;
    return $str;
}

sub saved { $saved{${$_[0]}} }
sub mark_saved { $saved{${$_[0]}} = 1 }
sub unmark_saved { $saved{${$_[0]}} = 0 }

sub debug { $debug_bc = shift }

sub pvix {	# save a shared PV (mainly for COPs)
    return $strtable{$_[0]} if defined($strtable{$_[0]});
    asmf "newpv %s\n", pvstring($_[0]);
    my $ix = $nextix++;
    $strtable{$_[0]} = $ix;
    asmf "stpv %d\n", $ix;
    return $ix;
}

sub B::OBJECT::nyi {
    my $obj = shift;
    warn sprintf("bytecode save method for %s (0x%x) not yet implemented\n",
		 class($obj), $$obj);
}

#
# objix may stomp on the op register (for op objects)
# or the sv register (for SV objects)
#
sub B::OBJECT::objix {
    my $obj = shift;
    my $ix = $symtable{$$obj};
    if (defined($ix)) {
	return $ix;
    } else {
	$obj->newix($nextix);
	return $symtable{$$obj} = $nextix++;
    }
}

sub B::SV::newix {
    my ($sv, $ix) = @_;
    asmf "newsv %d\t# %s\n", $sv->FLAGS & SVTYPEMASK, class($sv);
    stsv($ix);    
}

sub B::GV::newix {
    my ($gv, $ix) = @_;
    my $gvname = $gv->NAME;
    my $name = cstring($gv->STASH->NAME . "::" . $gvname);
    asm "gv_fetchpv $name\n";
    stsv($ix);
}

sub B::HV::newix {
    my ($hv, $ix) = @_;
    my $name = $hv->NAME;
    if ($name) {
	# It's a stash
	asmf "gv_stashpv %s\n", cstring($name);
	stsv($ix);
    } else {
	# It's an ordinary HV. Fall back to ordinary newix method
	$hv->B::SV::newix($ix);
    }
}

sub B::SPECIAL::newix {
    my ($sv, $ix) = @_;
    # Special case. $$sv is not the address of the SV but an
    # index into svspecialsv_list.
    asmf "ldspecsv $$sv\t# %s\n", $specialsv_name[$$sv];
    stsv($ix);
}

sub B::OP::newix {
    my ($op, $ix) = @_;
    my $class = class($op);
    my $typenum = $optype_enum{$class};
    croak("OP::newix: can't understand class $class") unless defined($typenum);
    asm "newop $typenum\t# $class\n";
    stop($ix);
}

sub B::OP::walkoptree_debug {
    my $op = shift;
    warn(sprintf("walkoptree: %s\n", peekop($op)));
}

sub B::OP::bytecode {
    my $op = shift;
    my $next = $op->next;
    my $nextix;
    my $sibix = $op->sibling->objix unless $strip_syntree;
    my $ix = $op->objix;
    my $type = $op->type;

    if ($bypass_nullops) {
	$next = $next->next while $$next && $next->type == 0;
    }
    $nextix = $next->objix;

    asmf "# %s\n", peekop($op) if $debug_bc;
    ldop($ix);
    asm "op_next $nextix\n";
    asm "op_sibling $sibix\n" unless $strip_syntree;
    asmf "op_type %s\t# %d\n", "pp_" . $op->name, $type;
    asmf("op_seq %d\n", $op->seq) unless $omit_seq;
    if ($type || !$compress_nullops) {
	asmf "op_targ %d\nop_flags 0x%x\nop_private 0x%x\n",
	    $op->targ, $op->flags, $op->private;
    }
}

sub B::UNOP::bytecode {
    my $op = shift;
    my $firstix = $op->first->objix unless $strip_syntree;
    $op->B::OP::bytecode;
    if (($op->type || !$compress_nullops) && !$strip_syntree) {
	asm "op_first $firstix\n";
    }
}

sub B::LOGOP::bytecode {
    my $op = shift;
    my $otherix = $op->other->objix;
    $op->B::UNOP::bytecode;
    asm "op_other $otherix\n";
}

sub B::SVOP::bytecode {
    my $op = shift;
    my $sv = $op->sv;
    my $svix = $sv->objix;
    $op->B::OP::bytecode;
    asm "op_sv $svix\n";
    $sv->bytecode;
}

sub B::PADOP::bytecode {
    my $op = shift;
    my $padix = $op->padix;
    $op->B::OP::bytecode;
    asm "op_padix $padix\n";
}

sub B::PVOP::bytecode {
    my $op = shift;
    my $pv = $op->pv;
    $op->B::OP::bytecode;
    #
    # This would be easy except that OP_TRANS uses a PVOP to store an
    # endian-dependent array of 256 shorts instead of a plain string.
    #
    if ($op->name eq "trans") {
	my @shorts = unpack("s256", $pv); # assembler handles endianness
	asm "op_pv_tr ", join(",", @shorts), "\n";
    } else {
	asmf "newpv %s\nop_pv\n", pvstring($pv);
    }
}

sub B::BINOP::bytecode {
    my $op = shift;
    my $lastix = $op->last->objix unless $strip_syntree;
    $op->B::UNOP::bytecode;
    if (($op->type || !$compress_nullops) && !$strip_syntree) {
	asm "op_last $lastix\n";
    }
}

sub B::LOOP::bytecode {
    my $op = shift;
    my $redoopix = $op->redoop->objix;
    my $nextopix = $op->nextop->objix;
    my $lastopix = $op->lastop->objix;
    $op->B::LISTOP::bytecode;
    asm "op_redoop $redoopix\nop_nextop $nextopix\nop_lastop $lastopix\n";
}

sub B::COP::bytecode {
    my $op = shift;
    my $file = $op->file;
    my $line = $op->line;
    if ($debug_bc) { # do this early to aid debugging
	asmf "# line %s:%d\n", $file, $line;
    }
    my $stashpv = $op->stashpv;
    my $warnings = $op->warnings;
    my $warningsix = $warnings->objix;
    my $labelix = pvix($op->label);
    my $stashix = pvix($stashpv);
    my $fileix = pvix($file);
    $warnings->bytecode;
    $op->B::OP::bytecode;
    asmf <<"EOT", $labelix, $stashix, $op->cop_seq, $fileix, $op->arybase;
cop_label %d
cop_stashpv %d
cop_seq %d
cop_file %d
cop_arybase %d
cop_line $line
cop_warnings $warningsix
EOT
}

sub B::PMOP::bytecode {
    my $op = shift;
    my $replroot = $op->pmreplroot;
    my $replrootix = $replroot->objix;
    my $replstartix = $op->pmreplstart->objix;
    my $opname = $op->name;
    # pmnext is corrupt in some PMOPs (see misc.t for example)
    #my $pmnextix = $op->pmnext->objix;

    if ($$replroot) {
	# OP_PUSHRE (a mutated version of OP_MATCH for the regexp
	# argument to a split) stores a GV in op_pmreplroot instead
	# of a substitution syntax tree. We don't want to walk that...
	if ($opname eq "pushre") {
	    $replroot->bytecode;
	} else {
	    walkoptree($replroot, "bytecode");
	}
    }
    $op->B::LISTOP::bytecode;
    if ($opname eq "pushre") {
	asmf "op_pmreplrootgv $replrootix\n";
    } else {
	asm "op_pmreplroot $replrootix\nop_pmreplstart $replstartix\n";
    }
    my $re = pvstring($op->precomp);
    # op_pmnext omitted since a perl bug means it's sometime corrupt
    asmf <<"EOT", $op->pmflags, $op->pmpermflags;
op_pmflags 0x%x
op_pmpermflags 0x%x
newpv $re
pregcomp
EOT
}

sub B::SV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $ix = $sv->objix;
    my $refcnt = $sv->REFCNT;
    my $flags = sprintf("0x%x", $sv->FLAGS);
    ldsv($ix);
    asm "sv_refcnt $refcnt\nsv_flags $flags\n";
    mark_saved($sv);
}

sub B::PV::bytecode {
    my $sv = shift;
    return if saved($sv);
    $sv->B::SV::bytecode;
    asmf("newpv %s\nxpv\n", pvstring($sv->PV)) if $sv->FLAGS & POK;
}

sub B::IV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $iv = $sv->IVX;
    $sv->B::SV::bytecode;
    asmf "%s $iv\n", $sv->needs64bits ? "xiv64" : "xiv32" if $sv->FLAGS & IOK; # could be PVNV
}

sub B::NV::bytecode {
    my $sv = shift;
    return if saved($sv);
    $sv->B::SV::bytecode;
    asmf "xnv %s\n", nv($sv->NVX);
}

sub B::RV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $rv = $sv->RV;
    my $rvix = $rv->objix;
    $rv->bytecode;
    $sv->B::SV::bytecode;
    asm "xrv $rvix\n";
}

sub B::PVIV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $iv = $sv->IVX;
    $sv->B::PV::bytecode;
    asmf "%s $iv\n", $sv->needs64bits ? "xiv64" : "xiv32";
}

sub B::PVNV::bytecode {
    my $sv = shift;
    my $flag = shift || 0;
    # The $flag argument is passed through PVMG::bytecode by BM::bytecode
    # and AV::bytecode and indicates special handling. $flag = 1 is used by
    # BM::bytecode and means that we should ensure we save the whole B-M
    # table. It consists of 257 bytes (256 char array plus a final \0)
    # which follow the ordinary PV+\0 and the 257 bytes are *not* reflected
    # in SvCUR. $flag = 2 is used by AV::bytecode and means that we only
    # call SV::bytecode instead of saving PV and calling NV::bytecode since
    # PV/NV/IV stuff is different for AVs.
    return if saved($sv);
    if ($flag == 2) {
	$sv->B::SV::bytecode;
    } else {
	my $pv = $sv->PV;
	$sv->B::IV::bytecode;
	asmf "xnv %s\n", nv($sv->NVX);
	if ($flag == 1) {
	    $pv .= "\0" . $sv->TABLE;
	    asmf "newpv %s\npv_cur %d\nxpv\n", pvstring($pv),length($pv)-257;
	} else {
	    asmf("newpv %s\nxpv\n", pvstring($pv)) if $sv->FLAGS & POK;
	}
    }
}

sub B::PVMG::bytecode {
    my ($sv, $flag) = @_;
    # See B::PVNV::bytecode for an explanation of $flag.
    return if saved($sv);
    # XXX We assume SvSTASH is already saved and don't save it later ourselves
    my $stashix = $sv->SvSTASH->objix;
    my @mgchain = $sv->MAGIC;
    my (@mgobjix, $mg);
    #
    # We need to traverse the magic chain and get objix for each OBJ
    # field *before* we do B::PVNV::bytecode since objix overwrites
    # the sv register. However, we need to write the magic-saving
    # bytecode *after* B::PVNV::bytecode since sv isn't initialised
    # to refer to $sv until then.
    #
    @mgobjix = map($_->OBJ->objix, @mgchain);
    $sv->B::PVNV::bytecode($flag);
    asm "xmg_stash $stashix\n";
    foreach $mg (@mgchain) {
	asmf "sv_magic %s\nmg_obj %d\nnewpv %s\nmg_pv\n",
	    cstring($mg->TYPE), shift(@mgobjix), pvstring($mg->PTR);
    }
}

sub B::PVLV::bytecode {
    my $sv = shift;
    return if saved($sv);
    $sv->B::PVMG::bytecode;
    asmf <<'EOT', $sv->TARGOFF, $sv->TARGLEN, cstring($sv->TYPE);
xlv_targoff %d
xlv_targlen %d
xlv_type %s
EOT
}

sub B::BM::bytecode {
    my $sv = shift;
    return if saved($sv);
    # See PVNV::bytecode for an explanation of what the argument does
    $sv->B::PVMG::bytecode(1);
    asmf "xbm_useful %d\nxbm_previous %d\nxbm_rare %d\n",
	$sv->USEFUL, $sv->PREVIOUS, $sv->RARE;
}

sub empty_gv {	# is a GV empty except for imported stuff?
    my $gv = shift;

    return 0 if ($gv->SV->FLAGS & SVTYPEMASK);	# sv not SVt_NULL
    my @subfield_names = qw(AV HV CV FORM IO);
    @subfield_names = grep {;
				no strict 'refs';
				!($gv->GvFLAGS & ${\"GVf_IMPORTED_$_"}->()) && ${$gv->$_()};
			} @subfield_names;
    return scalar @subfield_names;
}

sub B::GV::bytecode {
    my $gv = shift;
    return if saved($gv);
    return unless grep { $_ eq $gv->STASH->NAME; } @packages;
    return if $gv->NAME =~ m/^\(/;	# ignore overloads - they'll be rebuilt
    my $ix = $gv->objix;
    mark_saved($gv);
    ldsv($ix);
    asmf <<"EOT", $gv->FLAGS, $gv->GvFLAGS;
sv_flags 0x%x
xgv_flags 0x%x
EOT
    my $refcnt = $gv->REFCNT;
    asmf("sv_refcnt_add %d\n", $refcnt - 1) if $refcnt > 1;
    return if $gv->is_empty;
    asmf <<"EOT", $gv->LINE, pvix($gv->FILE);
gp_line %d
gp_file %d
EOT
    my $gvname = $gv->NAME;
    my $name = cstring($gv->STASH->NAME . "::" . $gvname);
    my $egv = $gv->EGV;
    my $egvix = $egv->objix;
    my $gvrefcnt = $gv->GvREFCNT;
    asmf("gp_refcnt_add %d\n", $gvrefcnt - 1) if $gvrefcnt > 1;
    if ($gvrefcnt > 1 &&  $ix != $egvix) {
	asm "gp_share $egvix\n";
    } else {
	if ($gvname !~ /^([^A-Za-z]|STDIN|STDOUT|STDERR|ARGV|SIG|ENV)$/) {
	    my $i;
	    my @subfield_names = qw(SV AV HV CV FORM IO);
	    @subfield_names = grep {;
					no strict 'refs';
					!($gv->GvFLAGS & ${\"GVf_IMPORTED_$_"}->());
				} @subfield_names;
	    my @subfields = map($gv->$_(), @subfield_names);
	    my @ixes = map($_->objix, @subfields);
	    # Reset sv register for $gv
	    ldsv($ix);
	    for ($i = 0; $i < @ixes; $i++) {
		asmf "gp_%s %d\n", lc($subfield_names[$i]), $ixes[$i];
	    }
	    # Now save all the subfields
	    my $sv;
	    foreach $sv (@subfields) {
		$sv->bytecode;
	    }
	}
    }
}

sub B::HV::bytecode {
    my $hv = shift;
    return if saved($hv);
    mark_saved($hv);
    my $name = $hv->NAME;
    my $ix = $hv->objix;
    if (!$name) {
	# It's an ordinary HV. Stashes have NAME set and need no further
	# saving beyond the gv_stashpv that $hv->objix already ensures.
	my @contents = $hv->ARRAY;
	my ($i, @ixes);
	for ($i = 1; $i < @contents; $i += 2) {
	    push(@ixes, $contents[$i]->objix);
	}
	for ($i = 1; $i < @contents; $i += 2) {
	    $contents[$i]->bytecode;
	}
	ldsv($ix);
	for ($i = 0; $i < @contents; $i += 2) {
	    asmf("newpv %s\nhv_store %d\n",
		   pvstring($contents[$i]), $ixes[$i / 2]);
	}
	asmf "sv_refcnt %d\nsv_flags 0x%x\n", $hv->REFCNT, $hv->FLAGS;
    }
}

sub B::AV::bytecode {
    my $av = shift;
    return if saved($av);
    my $ix = $av->objix;
    my $fill = $av->FILL;
    my $max = $av->MAX;
    my (@array, @ixes);
    if ($fill > -1) {
	@array = $av->ARRAY;
	@ixes = map($_->objix, @array);
	my $sv;
	foreach $sv (@array) {
	    $sv->bytecode;
	}
    }
    # See PVNV::bytecode for the meaning of the flag argument of 2.
    $av->B::PVMG::bytecode(2);
    # Recover sv register and set AvMAX and AvFILL to -1 (since we
    # create an AV with NEWSV and SvUPGRADE rather than doing newAV
    # which is what sets AvMAX and AvFILL.
    ldsv($ix);
    asmf "sv_flags 0x%x\n", $av->FLAGS & ~SVf_READONLY; # SvREADONLY_off($av) in case PADCONST
    asmf "xav_flags 0x%x\nxav_max -1\nxav_fill -1\n", $av->AvFLAGS;
    if ($fill > -1) {
	my $elix;
	foreach $elix (@ixes) {
	    asm "av_push $elix\n";
	}
    } else {
	if ($max > -1) {
	    asm "av_extend $max\n";
	}
    }
    asmf "sv_flags 0x%x\n", $av->FLAGS; # restore flags from above
}

sub B::CV::bytecode {
    my $cv = shift;
    return if saved($cv);
    return if ${$cv->GV} && ($cv->GV->GvFLAGS & GVf_IMPORTED_CV);
    my $fileix = pvix($cv->FILE);
    my $ix = $cv->objix;
    $cv->B::PVMG::bytecode;
    my $i;
    my @subfield_names = qw(ROOT START STASH GV PADLIST OUTSIDE);
    my @subfields = map($cv->$_(), @subfield_names);
    my @ixes = map($_->objix, @subfields);
    # Save OP tree from CvROOT (first element of @subfields)
    my $root = shift @subfields;
    if ($$root) {
	walkoptree($root, "bytecode");
    }
    # Reset sv register for $cv (since above ->objix calls stomped on it)
    ldsv($ix);
    for ($i = 0; $i < @ixes; $i++) {
	asmf "xcv_%s %d\n", lc($subfield_names[$i]), $ixes[$i];
    }
    asmf "xcv_depth %d\nxcv_flags 0x%x\n", $cv->DEPTH, $cv->CvFLAGS;
    asmf "xcv_file %d\n", $fileix;
    # Now save all the subfields (except for CvROOT which was handled
    # above) and CvSTART (now the initial element of @subfields).
    shift @subfields; # bye-bye CvSTART
    my $sv;
    foreach $sv (@subfields) {
	$sv->bytecode;
    }
}

sub B::IO::bytecode {
    my $io = shift;
    return if saved($io);
    my $ix = $io->objix;
    my $top_gv = $io->TOP_GV;
    my $top_gvix = $top_gv->objix;
    my $fmt_gv = $io->FMT_GV;
    my $fmt_gvix = $fmt_gv->objix;
    my $bottom_gv = $io->BOTTOM_GV;
    my $bottom_gvix = $bottom_gv->objix;

    $io->B::PVMG::bytecode;
    ldsv($ix);
    asm "xio_top_gv $top_gvix\n";
    asm "xio_fmt_gv $fmt_gvix\n";
    asm "xio_bottom_gv $bottom_gvix\n";
    my $field;
    foreach $field (qw(TOP_NAME FMT_NAME BOTTOM_NAME)) {
	asmf "newpv %s\nxio_%s\n", pvstring($io->$field()), lc($field);
    }
    foreach $field (qw(LINES PAGE PAGE_LEN LINES_LEFT SUBPROCESS)) {
	asmf "xio_%s %d\n", lc($field), $io->$field();
    }
    asmf "xio_type %s\nxio_flags 0x%x\n", cstring($io->IoTYPE), $io->IoFLAGS;
    $top_gv->bytecode;
    $fmt_gv->bytecode;
    $bottom_gv->bytecode;
}

sub B::SPECIAL::bytecode {
    # nothing extra needs doing
}

sub bytecompile_object {
    for my $sv (@_) {
	svref_2object($sv)->bytecode;
    }
}

sub B::GV::bytecodecv {
    my $gv = shift;
    my $cv = $gv->CV;
    if ($$cv && !saved($cv) && !($gv->FLAGS & GVf_IMPORTED_CV)) {
	if ($debug_cv) {
	    warn sprintf("saving extra CV &%s::%s (0x%x) from GV 0x%x\n",
			 $gv->STASH->NAME, $gv->NAME, $$cv, $$gv);
	}
	$gv->bytecode;
    }
}

sub save_call_queues {
    if (begin_av()->isa("B::AV")) {	# this is just to save 'use Foo;' calls
	for my $cv (begin_av()->ARRAY) {
	    next unless grep { $_ eq $cv->STASH->NAME; } @packages;
	    my $op = $cv->START;
OPLOOP:
	    while ($$op) {
	 	if ($op->name eq 'require') { # save any BEGIN that does a require
		    $cv->bytecode;
		    asmf "push_begin %d\n", $cv->objix;
		    last OPLOOP;
		}
		$op = $op->next;
	    }
	}
    }
    if (init_av()->isa("B::AV")) {
	for my $cv (init_av()->ARRAY) {
	    next unless grep { $_ eq $cv->STASH->NAME; } @packages;
	    $cv->bytecode;
	    asmf "push_init %d\n", $cv->objix;
	}
    }
    if (end_av()->isa("B::AV")) {
	for my $cv (end_av()->ARRAY) {
	    next unless grep { $_ eq $cv->STASH->NAME; } @packages;
	    $cv->bytecode;
	    asmf "push_end %d\n", $cv->objix;
	}
    }
}

sub symwalk {
    no strict 'refs';
    my $ok = 1 if grep { (my $name = $_[0]) =~ s/::$//; $_ eq $name;} @packages;
    if (grep { /^$_[0]/; } @packages) {
	walksymtable(\%{"$_[0]"}, "bytecodecv", \&symwalk, $_[0]);
    }
    warn "considering $_[0] ... " . ($ok ? "accepted\n" : "rejected\n")
	if $debug_bc;
    $ok;
}

sub bytecompile_main {
    my $curpad = (comppadlist->ARRAY)[1];
    my $curpadix = $curpad->objix;
    $curpad->bytecode;
    save_call_queues();
    walkoptree(main_root, "bytecode") unless ref(main_root) eq "B::NULL";
    warn "done main program, now walking symbol table\n" if $debug_bc;
    if (@packages) {
	no strict qw(refs);
	walksymtable(\%{"main::"}, "bytecodecv", \&symwalk);
    } else {
	die "No packages requested for compilation!\n";
    }
    asmf "main_root %d\n", main_root->objix;
    asmf "main_start %d\n", main_start->objix;
    asmf "curpad $curpadix\n";
    # XXX Do min_intro_pending and max_intro_pending matter?
}

sub compile {
    my @options = @_;
    my ($option, $opt, $arg);
    open(OUT, ">&STDOUT");
    binmode OUT;
    select OUT;
  OPTION:
    while ($option = shift @options) {
	if ($option =~ /^-(.)(.*)/) {
	    $opt = $1;
	    $arg = $2;
	} else {
	    unshift @options, $option;
	    last OPTION;
	}
	if ($opt eq "-" && $arg eq "-") {
	    shift @options;
	    last OPTION;
	} elsif ($opt eq "o") {
	    $arg ||= shift @options;
	    open(OUT, ">$arg") or return "$arg: $!\n";
	    binmode OUT;
	} elsif ($opt eq "a") {
	    $arg ||= shift @options;
	    open(OUT, ">>$arg") or return "$arg: $!\n";
	    binmode OUT;
	} elsif ($opt eq "D") {
	    $arg ||= shift @options;
	    foreach $arg (split(//, $arg)) {
		if ($arg eq "b") {
		    $| = 1;
		    debug(1);
		} elsif ($arg eq "o") {
		    B->debug(1);
		} elsif ($arg eq "a") {
		    B::Assembler::debug(1);
		} elsif ($arg eq "C") {
		    $debug_cv = 1;
		}
	    }
	} elsif ($opt eq "v") {
	    $verbose = 1;
	} elsif ($opt eq "S") {
	    $no_assemble = 1;
	} elsif ($opt eq "f") {
	    $arg ||= shift @options;
	    my $value = $arg !~ s/^no-//;
	    $arg =~ s/-/_/g;
	    my $ref = $optimise{$arg};
	    if (defined($ref)) {
		$$ref = $value;
	    } else {
		warn qq(ignoring unknown optimisation option "$arg"\n);
	    }
	} elsif ($opt eq "O") {
	    $arg = 1 if $arg eq "";
	    my $ref;
	    foreach $ref (values %optimise) {
		$$ref = 0;
	    }
	    if ($arg >= 2) {
		$bypass_nullops = 1;
	    }
	    if ($arg >= 1) {
		$compress_nullops = 1;
		$omit_seq = 1;
	    }
	} elsif ($opt eq "u") {
	    $arg ||= shift @options;
	    push @packages, $arg;
	} else {
	    warn qq(ignoring unknown option "$opt$arg"\n);
	}
    }
    if (! @packages) {
	warn "No package specified for compilation, assuming main::\n";
	@packages = qw(main);
    }
    if (@options) {
	die "Extraneous options left on B::Bytecode commandline: @options\n";
    } else {
	return sub { 
	    newasm(\&apr) unless $no_assemble;
	    bytecompile_main();
	    endasm() unless $no_assemble;
	};
    }
}

sub apr { print @_; }

1;

__END__

=head1 NAME

B::Bytecode - Perl compiler's bytecode backend

=head1 SYNOPSIS

	perl -MO=Bytecode[,OPTIONS] foo.pl

=head1 DESCRIPTION

This compiler backend takes Perl source and generates a
platform-independent bytecode encapsulating code to load the
internal structures perl uses to run your program. When the
generated bytecode is loaded in, your program is ready to run,
reducing the time which perl would have taken to load and parse
your program into its internal semi-compiled form. That means that
compiling with this backend will not help improve the runtime
execution speed of your program but may improve the start-up time.
Depending on the environment in which your program runs this may
or may not be a help.

The resulting bytecode can be run with a special byteperl executable
or (for non-main programs) be loaded via the C<byteload_fh> function
in the F<B> module.

=head1 OPTIONS

If there are any non-option arguments, they are taken to be names of
objects to be saved (probably doesn't work properly yet).  Without
extra arguments, it saves the main program.

=over 4

=item B<-ofilename>

Output to filename instead of STDOUT.

=item B<-afilename>

Append output to filename.

=item B<-->

Force end of options.

=item B<-f>

Force optimisations on or off one at a time. Each can be preceded
by B<no-> to turn the option off (e.g. B<-fno-compress-nullops>).

=item B<-fcompress-nullops>

Only fills in the necessary fields of ops which have
been optimised away by perl's internal compiler.

=item B<-fomit-sequence-numbers>

Leaves out code to fill in the op_seq field of all ops
which is only used by perl's internal compiler.

=item B<-fbypass-nullops>

If op->op_next ever points to a NULLOP, replaces the op_next field
with the first non-NULLOP in the path of execution.

=item B<-On>

Optimisation level (n = 0, 1, 2, ...). B<-O> means B<-O1>.
B<-O1> sets B<-fcompress-nullops> B<-fomit-sequence numbers>.
B<-O2> adds B<-fbypass-nullops>.

=item B<-D>

Debug options (concatenated or separate flags like C<perl -D>).

=item B<-Do>

Prints each OP as it's processed.

=item B<-Db>

Print debugging information about bytecompiler progress.

=item B<-Da>

Tells the (bytecode) assembler to include source assembler lines
in its output as bytecode comments.

=item B<-DC>

Prints each CV taken from the final symbol tree walk.

=item B<-S>

Output (bytecode) assembler source rather than piping it
through the assembler and outputting bytecode.

=item B<-upackage>
  
Stores package in the output.
  
=back

=head1 EXAMPLES

    perl -MO=Bytecode,-O6,-ofoo.plc,-umain foo.pl

    perl -MO=Bytecode,-S,-umain foo.pl > foo.S
    assemble foo.S > foo.plc

Note that C<assemble> lives in the C<B> subdirectory of your perl
library directory. The utility called perlcc may also be used to 
help make use of this compiler.

    perl -MO=Bytecode,-uFoo,-oFoo.pmc Foo.pm

=head1 BUGS

Output is still huge and there are still occasional crashes during
either compilation or ByteLoading. Current status: experimental.

=head1 AUTHORS

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>
Benjamin Stuhl, C<sho_pi@hotmail.com>

=cut
