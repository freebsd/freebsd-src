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
use IO::File;

use B qw(minus_c main_cv main_root main_start comppadlist
	 class peekop walkoptree svref_2object cstring walksymtable);
use B::Asmdata qw(@optype @specialsv_name);
use B::Assembler qw(assemble_fh);

my %optype_enum;
my $i;
for ($i = 0; $i < @optype; $i++) {
    $optype_enum{$optype[$i]} = $i;
}

# Following is SVf_POK|SVp_POK
# XXX Shouldn't be hardwired
sub POK () { 0x04040000 }

# Following is SVf_IOK|SVp_OK
# XXX Shouldn't be hardwired
sub IOK () { 0x01010000 }

my ($verbose, $module_only, $no_assemble, $debug_bc, $debug_cv);
my $assembler_pid;

# Optimisation options. On the command line, use hyphens instead of
# underscores for compatibility with gcc-style options. We use
# underscores here because they are OK in (strict) barewords.
my ($strip_syntree, $compress_nullops, $omit_seq, $bypass_nullops);
my %optimise = (strip_syntax_tree	=> \$strip_syntree,
		compress_nullops	=> \$compress_nullops,
		omit_sequence_numbers	=> \$omit_seq,
		bypass_nullops		=> \$bypass_nullops);

my $nextix = 0;
my %symtable;	# maps object addresses to object indices.
		# Filled in at allocation (newsv/newop) time.
my %saved;	# maps object addresses (for SVish classes) to "saved yet?"
		# flag. Set at FOO::bytecode time usually by SV::bytecode.
		# Manipulated via saved(), mark_saved(), unmark_saved().

my $svix = -1;	# we keep track of when the sv register contains an element
		# of the object table to avoid unnecessary repeated
		# consecutive ldsv instructions.
my $opix = -1;	# Ditto for the op register.

sub ldsv {
    my $ix = shift;
    if ($ix != $svix) {
	print "ldsv $ix\n";
	$svix = $ix;
    }
}

sub stsv {
    my $ix = shift;
    print "stsv $ix\n";
    $svix = $ix;
}

sub set_svix {
    $svix = shift;
}

sub ldop {
    my $ix = shift;
    if ($ix != $opix) {
	print "ldop $ix\n";
	$opix = $ix;
    }
}

sub stop {
    my $ix = shift;
    print "stop $ix\n";
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

sub saved { $saved{${$_[0]}} }
sub mark_saved { $saved{${$_[0]}} = 1 }
sub unmark_saved { $saved{${$_[0]}} = 0 }

sub debug { $debug_bc = shift }

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
    printf "newsv %d\t# %s\n", $sv->FLAGS & 0xf, class($sv);
    stsv($ix);    
}

sub B::GV::newix {
    my ($gv, $ix) = @_;
    my $gvname = $gv->NAME;
    my $name = cstring($gv->STASH->NAME . "::" . $gvname);
    print "gv_fetchpv $name\n";
    stsv($ix);
}

sub B::HV::newix {
    my ($hv, $ix) = @_;
    my $name = $hv->NAME;
    if ($name) {
	# It's a stash
	printf "gv_stashpv %s\n", cstring($name);
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
    printf "ldspecsv $$sv\t# %s\n", $specialsv_name[$$sv];
    stsv($ix);
}

sub B::OP::newix {
    my ($op, $ix) = @_;
    my $class = class($op);
    my $typenum = $optype_enum{$class};
    croak "OP::newix: can't understand class $class" unless defined($typenum);
    print "newop $typenum\t# $class\n";
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
    my $sibix = $op->sibling->objix;
    my $ix = $op->objix;
    my $type = $op->type;

    if ($bypass_nullops) {
	$next = $next->next while $$next && $next->type == 0;
    }
    $nextix = $next->objix;

    printf "# %s\n", peekop($op) if $debug_bc;
    ldop($ix);
    print "op_next $nextix\n";
    print "op_sibling $sibix\n" unless $strip_syntree;
    printf "op_type %s\t# %d\n", $op->ppaddr, $type;
    printf("op_seq %d\n", $op->seq) unless $omit_seq;
    if ($type || !$compress_nullops) {
	printf "op_targ %d\nop_flags 0x%x\nop_private 0x%x\n",
	    $op->targ, $op->flags, $op->private;
    }
}

sub B::UNOP::bytecode {
    my $op = shift;
    my $firstix = $op->first->objix;
    $op->B::OP::bytecode;
    if (($op->type || !$compress_nullops) && !$strip_syntree) {
	print "op_first $firstix\n";
    }
}

sub B::LOGOP::bytecode {
    my $op = shift;
    my $otherix = $op->other->objix;
    $op->B::UNOP::bytecode;
    print "op_other $otherix\n";
}

sub B::SVOP::bytecode {
    my $op = shift;
    my $sv = $op->sv;
    my $svix = $sv->objix;
    $op->B::OP::bytecode;
    print "op_sv $svix\n";
    $sv->bytecode;
}

sub B::GVOP::bytecode {
    my $op = shift;
    my $gv = $op->gv;
    my $gvix = $gv->objix;
    $op->B::OP::bytecode;
    print "op_gv $gvix\n";
    $gv->bytecode;
}

sub B::PVOP::bytecode {
    my $op = shift;
    my $pv = $op->pv;
    $op->B::OP::bytecode;
    #
    # This would be easy except that OP_TRANS uses a PVOP to store an
    # endian-dependent array of 256 shorts instead of a plain string.
    #
    if ($op->ppaddr eq "pp_trans") {
	my @shorts = unpack("s256", $pv); # assembler handles endianness
	print "op_pv_tr ", join(",", @shorts), "\n";
    } else {
	printf "newpv %s\nop_pv\n", pvstring($pv);
    }
}

sub B::BINOP::bytecode {
    my $op = shift;
    my $lastix = $op->last->objix;
    $op->B::UNOP::bytecode;
    if (($op->type || !$compress_nullops) && !$strip_syntree) {
	print "op_last $lastix\n";
    }
}

sub B::CONDOP::bytecode {
    my $op = shift;
    my $trueix = $op->true->objix;
    my $falseix = $op->false->objix;
    $op->B::UNOP::bytecode;
    print "op_true $trueix\nop_false $falseix\n";
}

sub B::LISTOP::bytecode {
    my $op = shift;
    my $children = $op->children;
    $op->B::BINOP::bytecode;
    if (($op->type || !$compress_nullops) && !$strip_syntree) {
	print "op_children $children\n";
    }
}

sub B::LOOP::bytecode {
    my $op = shift;
    my $redoopix = $op->redoop->objix;
    my $nextopix = $op->nextop->objix;
    my $lastopix = $op->lastop->objix;
    $op->B::LISTOP::bytecode;
    print "op_redoop $redoopix\nop_nextop $nextopix\nop_lastop $lastopix\n";
}

sub B::COP::bytecode {
    my $op = shift;
    my $stash = $op->stash;
    my $stashix = $stash->objix;
    my $filegv = $op->filegv;
    my $filegvix = $filegv->objix;
    my $line = $op->line;
    if ($debug_bc) {
	printf "# line %s:%d\n", $filegv->SV->PV, $line;
    }
    $op->B::OP::bytecode;
    printf <<"EOT", pvstring($op->label), $op->cop_seq, $op->arybase;
newpv %s
cop_label
cop_stash $stashix
cop_seq %d
cop_filegv $filegvix
cop_arybase %d
cop_line $line
EOT
    $filegv->bytecode;
    $stash->bytecode;
}

sub B::PMOP::bytecode {
    my $op = shift;
    my $replroot = $op->pmreplroot;
    my $replrootix = $replroot->objix;
    my $replstartix = $op->pmreplstart->objix;
    my $ppaddr = $op->ppaddr;
    # pmnext is corrupt in some PMOPs (see misc.t for example)
    #my $pmnextix = $op->pmnext->objix;

    if ($$replroot) {
	# OP_PUSHRE (a mutated version of OP_MATCH for the regexp
	# argument to a split) stores a GV in op_pmreplroot instead
	# of a substitution syntax tree. We don't want to walk that...
	if ($ppaddr eq "pp_pushre") {
	    $replroot->bytecode;
	} else {
	    walkoptree($replroot, "bytecode");
	}
    }
    $op->B::LISTOP::bytecode;
    if ($ppaddr eq "pp_pushre") {
	printf "op_pmreplrootgv $replrootix\n";
    } else {
	print "op_pmreplroot $replrootix\nop_pmreplstart $replstartix\n";
    }
    my $re = pvstring($op->precomp);
    # op_pmnext omitted since a perl bug means it's sometime corrupt
    printf <<"EOT", $op->pmflags, $op->pmpermflags;
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
    print "sv_refcnt $refcnt\nsv_flags $flags\n";
    mark_saved($sv);
}

sub B::PV::bytecode {
    my $sv = shift;
    return if saved($sv);
    $sv->B::SV::bytecode;
    printf("newpv %s\nxpv\n", pvstring($sv->PV)) if $sv->FLAGS & POK;
}

sub B::IV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $iv = $sv->IVX;
    $sv->B::SV::bytecode;
    printf "%s $iv\n", $sv->needs64bits ? "xiv64" : "xiv32";
}

sub B::NV::bytecode {
    my $sv = shift;
    return if saved($sv);
    $sv->B::SV::bytecode;
    printf "xnv %s\n", $sv->NVX;
}

sub B::RV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $rv = $sv->RV;
    my $rvix = $rv->objix;
    $rv->bytecode;
    $sv->B::SV::bytecode;
    print "xrv $rvix\n";
}

sub B::PVIV::bytecode {
    my $sv = shift;
    return if saved($sv);
    my $iv = $sv->IVX;
    $sv->B::PV::bytecode;
    printf "%s $iv\n", $sv->needs64bits ? "xiv64" : "xiv32";
}

sub B::PVNV::bytecode {
    my ($sv, $flag) = @_;
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
	printf "xnv %s\n", $sv->NVX;
	if ($flag == 1) {
	    $pv .= "\0" . $sv->TABLE;
	    printf "newpv %s\npv_cur %d\nxpv\n", pvstring($pv),length($pv)-257;
	} else {
	    printf("newpv %s\nxpv\n", pvstring($pv)) if $sv->FLAGS & POK;
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
    print "xmg_stash $stashix\n";
    foreach $mg (@mgchain) {
	printf "sv_magic %s\nmg_obj %d\nnewpv %s\nmg_pv\n",
	    cstring($mg->TYPE), shift(@mgobjix), pvstring($mg->PTR);
    }
}

sub B::PVLV::bytecode {
    my $sv = shift;
    return if saved($sv);
    $sv->B::PVMG::bytecode;
    printf <<'EOT', $sv->TARGOFF, $sv->TARGLEN, cstring($sv->TYPE);
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
    printf "xbm_useful %d\nxbm_previous %d\nxbm_rare %d\n",
	$sv->USEFUL, $sv->PREVIOUS, $sv->RARE;
}

sub B::GV::bytecode {
    my $gv = shift;
    return if saved($gv);
    my $ix = $gv->objix;
    mark_saved($gv);
    my $gvname = $gv->NAME;
    my $name = cstring($gv->STASH->NAME . "::" . $gvname);
    my $egv = $gv->EGV;
    my $egvix = $egv->objix;
    ldsv($ix);
    printf <<"EOT", $gv->FLAGS, $gv->GvFLAGS, $gv->LINE;
sv_flags 0x%x
xgv_flags 0x%x
gp_line %d
EOT
    my $refcnt = $gv->REFCNT;
    printf("sv_refcnt_add %d\n", $refcnt - 1) if $refcnt > 1;
    my $gvrefcnt = $gv->GvREFCNT;
    printf("gp_refcnt_add %d\n", $gvrefcnt - 1) if $gvrefcnt > 1;
    if ($gvrefcnt > 1 &&  $ix != $egvix) {
	print "gp_share $egvix\n";
    } else {
	if ($gvname !~ /^([^A-Za-z]|STDIN|STDOUT|STDERR|ARGV|SIG|ENV)$/) {
	    my $i;
	    my @subfield_names = qw(SV AV HV CV FILEGV FORM IO);
	    my @subfields = map($gv->$_(), @subfield_names);
	    my @ixes = map($_->objix, @subfields);
	    # Reset sv register for $gv
	    ldsv($ix);
	    for ($i = 0; $i < @ixes; $i++) {
		printf "gp_%s %d\n", lc($subfield_names[$i]), $ixes[$i];
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
	    printf("newpv %s\nhv_store %d\n",
		   pvstring($contents[$i]), $ixes[$i / 2]);
	}
	printf "sv_refcnt %d\nsv_flags 0x%x\n", $hv->REFCNT, $hv->FLAGS;
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
    printf "xav_flags 0x%x\nxav_max -1\nxav_fill -1\n", $av->AvFLAGS;
    if ($fill > -1) {
	my $elix;
	foreach $elix (@ixes) {
	    print "av_push $elix\n";
	}
    } else {
	if ($max > -1) {
	    print "av_extend $max\n";
	}
    }
}

sub B::CV::bytecode {
    my $cv = shift;
    return if saved($cv);
    my $ix = $cv->objix;
    $cv->B::PVMG::bytecode;
    my $i;
    my @subfield_names = qw(ROOT START STASH GV FILEGV PADLIST OUTSIDE);
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
	printf "xcv_%s %d\n", lc($subfield_names[$i]), $ixes[$i];
    }
    printf "xcv_depth %d\nxcv_flags 0x%x\n", $cv->DEPTH, $cv->FLAGS;
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
    print "xio_top_gv $top_gvix\n";
    print "xio_fmt_gv $fmt_gvix\n";
    print "xio_bottom_gv $bottom_gvix\n";
    my $field;
    foreach $field (qw(TOP_NAME FMT_NAME BOTTOM_NAME)) {
	printf "newpv %s\nxio_%s\n", pvstring($io->$field()), lc($field);
    }
    foreach $field (qw(LINES PAGE PAGE_LEN LINES_LEFT SUBPROCESS)) {
	printf "xio_%s %d\n", lc($field), $io->$field();
    }
    printf "xio_type %s\nxio_flags 0x%x\n", cstring($io->IoTYPE), $io->IoFLAGS;
    $top_gv->bytecode;
    $fmt_gv->bytecode;
    $bottom_gv->bytecode;
}

sub B::SPECIAL::bytecode {
    # nothing extra needs doing
}

sub bytecompile_object {
    my $sv;
    foreach $sv (@_) {
	svref_2object($sv)->bytecode;
    }
}

sub B::GV::bytecodecv {
    my $gv = shift;
    my $cv = $gv->CV;
    if ($$cv && !saved($cv)) {
	if ($debug_cv) {
	    warn sprintf("saving extra CV &%s::%s (0x%x) from GV 0x%x\n",
			 $gv->STASH->NAME, $gv->NAME, $$cv, $$gv);
	}
	$gv->bytecode;
    }
}

sub bytecompile_main {
    my $curpad = (comppadlist->ARRAY)[1];
    my $curpadix = $curpad->objix;
    $curpad->bytecode;
    walkoptree(main_root, "bytecode");
    warn "done main program, now walking symbol table\n" if $debug_bc;
    my ($pack, %exclude);
    foreach $pack (qw(B O AutoLoader DynaLoader Config DB VMS strict vars
		      FileHandle Exporter Carp UNIVERSAL IO Fcntl Symbol
		      SelectSaver blib Cwd))
    {
	$exclude{$pack."::"} = 1;
    }
    no strict qw(vars refs);
    walksymtable(\%{"main::"}, "bytecodecv", sub {
	warn "considering $_[0]\n" if $debug_bc;
	return !defined($exclude{$_[0]});
    });
    if (!$module_only) {
	printf "main_root %d\n", main_root->objix;
	printf "main_start %d\n", main_start->objix;
	printf "curpad $curpadix\n";
	# XXX Do min_intro_pending and max_intro_pending matter?
    }
}

sub prepare_assemble {
    my $newfh = IO::File->new_tmpfile;
    select($newfh);
    binmode $newfh;
    return $newfh;
}

sub do_assemble {
    my $fh = shift;
    seek($fh, 0, 0); # rewind the temporary file
    assemble_fh($fh, sub { print OUT @_ });
}

sub compile {
    my @options = @_;
    my ($option, $opt, $arg);
    open(OUT, ">&STDOUT");
    binmode OUT;
    select(OUT);
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
	} elsif ($opt eq "m") {
	    $module_only = 1;
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
	    if ($arg >= 6) {
		$strip_syntree = 1;
	    }
	    if ($arg >= 2) {
		$bypass_nullops = 1;
	    }
	    if ($arg >= 1) {
		$compress_nullops = 1;
		$omit_seq = 1;
	    }
	}
    }
    if (@options) {
	return sub {
	    my $objname;
	    my $newfh; 
	    $newfh = prepare_assemble() unless $no_assemble;
	    foreach $objname (@options) {
		eval "bytecompile_object(\\$objname)";
	    }
	    do_assemble($newfh) unless $no_assemble;
	}
    } else {
	return sub {
	    my $newfh; 
	    $newfh = prepare_assemble() unless $no_assemble;
	    bytecompile_main();
	    do_assemble($newfh) unless $no_assemble;
	}
    }
}

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

=item B<-fstrip-syntax-tree>

Leaves out code to fill in the pointers which link the internal syntax
tree together. They're not needed at run-time but leaving them out
will make it impossible to recompile or disassemble the resulting
program.  It will also stop C<goto label> statements from working.

=item B<-On>

Optimisation level (n = 0, 1, 2, ...). B<-O> means B<-O1>.
B<-O1> sets B<-fcompress-nullops> B<-fomit-sequence numbers>.
B<-O6> adds B<-fstrip-syntax-tree>.

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

=item B<-m>

Compile as a module rather than a standalone program. Currently this
just means that the bytecodes for initialising C<main_start>,
C<main_root> and C<curpad> are omitted.

=back

=head1 EXAMPLES

        perl -MO=Bytecode,-O6,-o,foo.plc foo.pl

        perl -MO=Bytecode,-S foo.pl > foo.S
        assemble foo.S > foo.plc
        byteperl foo.plc

        perl -MO=Bytecode,-m,-oFoo.pmc Foo.pm

=head1 BUGS

Plenty. Current status: experimental.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
