#      C.pm
#
#      Copyright (c) 1996, 1997, 1998 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
#
package B::C;
use Exporter ();
@ISA = qw(Exporter);
@EXPORT_OK = qw(output_all output_boilerplate output_main
		init_sections set_callback save_unused_subs objsym);

use B qw(minus_c sv_undef walkoptree walksymtable main_root main_start peekop
	 class cstring cchar svref_2object compile_stats comppadlist hash
	 threadsv_names main_cv init_av);
use B::Asmdata qw(@specialsv_name);

use FileHandle;
use Carp;
use strict;

my $hv_index = 0;
my $gv_index = 0;
my $re_index = 0;
my $pv_index = 0;
my $anonsub_index = 0;

my %symtable;
my $warn_undefined_syms;
my $verbose;
my @unused_sub_packages;
my $nullop_count;
my $pv_copy_on_grow;
my ($debug_cops, $debug_av, $debug_cv, $debug_mg);

my @threadsv_names;
BEGIN {
    @threadsv_names = threadsv_names();
}

# Code sections
my ($init, $decl, $symsect, $binopsect, $condopsect, $copsect, $cvopsect,
    $gvopsect, $listopsect, $logopsect, $loopsect, $opsect, $pmopsect,
    $pvopsect, $svopsect, $unopsect, $svsect, $xpvsect, $xpvavsect,
    $xpvhvsect, $xpvcvsect, $xpvivsect, $xpvnvsect, $xpvmgsect, $xpvlvsect,
    $xrvsect, $xpvbmsect, $xpviosect, $bootstrap);

sub walk_and_save_optree;
my $saveoptree_callback = \&walk_and_save_optree;
sub set_callback { $saveoptree_callback = shift }
sub saveoptree { &$saveoptree_callback(@_) }

sub walk_and_save_optree {
    my ($name, $root, $start) = @_;
    walkoptree($root, "save");
    return objsym($start);
}

# Current workaround/fix for op_free() trying to free statically
# defined OPs is to set op_seq = -1 and check for that in op_free().
# Instead of hardwiring -1 in place of $op->seq, we use $op_seq
# so that it can be changed back easily if necessary. In fact, to
# stop compilers from moaning about a U16 being initialised with an
# uncast -1 (the printf format is %d so we can't tweak it), we have
# to "know" that op_seq is a U16 and use 65535. Ugh.
my $op_seq = 65535;

sub AVf_REAL () { 1 }

# XXX This shouldn't really be hardcoded here but it saves
# looking up the name of every BASEOP in B::OP
sub OP_THREADSV () { 345 }

sub savesym {
    my ($obj, $value) = @_;
    my $sym = sprintf("s\\_%x", $$obj);
    $symtable{$sym} = $value;
}

sub objsym {
    my $obj = shift;
    return $symtable{sprintf("s\\_%x", $$obj)};
}

sub getsym {
    my $sym = shift;
    my $value;

    return 0 if $sym eq "sym_0";	# special case
    $value = $symtable{$sym};
    if (defined($value)) {
	return $value;
    } else {
	warn "warning: undefined symbol $sym\n" if $warn_undefined_syms;
	return "UNUSED";
    }
}

sub savepv {
    my $pv = shift;
    my $pvsym = 0;
    my $pvmax = 0;
    if ($pv_copy_on_grow) {
	my $cstring = cstring($pv);
	if ($cstring ne "0") { # sic
	    $pvsym = sprintf("pv%d", $pv_index++);
	    $decl->add(sprintf("static char %s[] = %s;", $pvsym, $cstring));
	}
    } else {
	$pvmax = length($pv) + 1;
    }
    return ($pvsym, $pvmax);
}

sub B::OP::save {
    my ($op, $level) = @_;
    my $type = $op->type;
    $nullop_count++ unless $type;
    if ($type == OP_THREADSV) {
	# saves looking up ppaddr but it's a bit naughty to hard code this
	$init->add(sprintf("(void)find_threadsv(%s);",
			   cstring($threadsv_names[$op->targ])));
    }
    $opsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x",
			 ${$op->next}, ${$op->sibling}, $op->ppaddr, $op->targ,
			 $type, $op_seq, $op->flags, $op->private));
    savesym($op, sprintf("&op_list[%d]", $opsect->index));
}

sub B::FAKEOP::new {
    my ($class, %objdata) = @_;
    bless \%objdata, $class;
}

sub B::FAKEOP::save {
    my ($op, $level) = @_;
    $opsect->add(sprintf("%s, %s, %s, %u, %u, %u, 0x%x, 0x%x",
			 $op->next, $op->sibling, $op->ppaddr, $op->targ,
			 $op->type, $op_seq, $op->flags, $op->private));
    return sprintf("&op_list[%d]", $opsect->index);
}

sub B::FAKEOP::next { $_[0]->{"next"} || 0 }
sub B::FAKEOP::type { $_[0]->{type} || 0}
sub B::FAKEOP::sibling { $_[0]->{sibling} || 0 }
sub B::FAKEOP::ppaddr { $_[0]->{ppaddr} || 0 }
sub B::FAKEOP::targ { $_[0]->{targ} || 0 }
sub B::FAKEOP::flags { $_[0]->{flags} || 0 }
sub B::FAKEOP::private { $_[0]->{private} || 0 }

sub B::UNOP::save {
    my ($op, $level) = @_;
    $unopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x",
			   ${$op->next}, ${$op->sibling}, $op->ppaddr,
			   $op->targ, $op->type, $op_seq, $op->flags,
			   $op->private, ${$op->first}));
    savesym($op, sprintf("(OP*)&unop_list[%d]", $unopsect->index));
}

sub B::BINOP::save {
    my ($op, $level) = @_;
    $binopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x, s\\_%x",
			    ${$op->next}, ${$op->sibling}, $op->ppaddr,
			    $op->targ, $op->type, $op_seq, $op->flags,
			    $op->private, ${$op->first}, ${$op->last}));
    savesym($op, sprintf("(OP*)&binop_list[%d]", $binopsect->index));
}

sub B::LISTOP::save {
    my ($op, $level) = @_;
    $listopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x, s\\_%x, %u",
			     ${$op->next}, ${$op->sibling}, $op->ppaddr,
			     $op->targ, $op->type, $op_seq, $op->flags,
			     $op->private, ${$op->first}, ${$op->last},
			     $op->children));
    savesym($op, sprintf("(OP*)&listop_list[%d]", $listopsect->index));
}

sub B::LOGOP::save {
    my ($op, $level) = @_;
    $logopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x, s\\_%x",
			    ${$op->next}, ${$op->sibling}, $op->ppaddr,
			    $op->targ, $op->type, $op_seq, $op->flags,
			    $op->private, ${$op->first}, ${$op->other}));
    savesym($op, sprintf("(OP*)&logop_list[%d]", $logopsect->index));
}

sub B::CONDOP::save {
    my ($op, $level) = @_;
    $condopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x, s\\_%x, s\\_%x",
			     ${$op->next}, ${$op->sibling}, $op->ppaddr,
			     $op->targ, $op->type, $op_seq, $op->flags,
			     $op->private, ${$op->first}, ${$op->true},
			     ${$op->false}));
    savesym($op, sprintf("(OP*)&condop_list[%d]", $condopsect->index));
}

sub B::LOOP::save {
    my ($op, $level) = @_;
    #warn sprintf("LOOP: redoop %s, nextop %s, lastop %s\n",
    #		 peekop($op->redoop), peekop($op->nextop),
    #		 peekop($op->lastop)); # debug
    $loopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x, s\\_%x, %u, s\\_%x, s\\_%x, s\\_%x",
			   ${$op->next}, ${$op->sibling}, $op->ppaddr,
			   $op->targ, $op->type, $op_seq, $op->flags,
			   $op->private, ${$op->first}, ${$op->last},
			   $op->children, ${$op->redoop}, ${$op->nextop},
			   ${$op->lastop}));
    savesym($op, sprintf("(OP*)&loop_list[%d]", $loopsect->index));
}

sub B::PVOP::save {
    my ($op, $level) = @_;
    $pvopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, %s",
			   ${$op->next}, ${$op->sibling}, $op->ppaddr,
			   $op->targ, $op->type, $op_seq, $op->flags,
			   $op->private, cstring($op->pv)));
    savesym($op, sprintf("(OP*)&pvop_list[%d]", $pvopsect->index));
}

sub B::SVOP::save {
    my ($op, $level) = @_;
    my $svsym = $op->sv->save;
    $svopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, %s",
			   ${$op->next}, ${$op->sibling}, $op->ppaddr,
			   $op->targ, $op->type, $op_seq, $op->flags,
			   $op->private, "(SV*)$svsym"));
    savesym($op, sprintf("(OP*)&svop_list[%d]", $svopsect->index));
}

sub B::GVOP::save {
    my ($op, $level) = @_;
    my $gvsym = $op->gv->save;
    $gvopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, Nullgv",
			   ${$op->next}, ${$op->sibling}, $op->ppaddr,
			   $op->targ, $op->type, $op_seq, $op->flags,
			   $op->private));
    $init->add(sprintf("gvop_list[%d].op_gv = %s;", $gvopsect->index, $gvsym));
    savesym($op, sprintf("(OP*)&gvop_list[%d]", $gvopsect->index));
}

sub B::COP::save {
    my ($op, $level) = @_;
    my $gvsym = $op->filegv->save;
    my $stashsym = $op->stash->save;
    warn sprintf("COP: line %d file %s\n", $op->line, $op->filegv->SV->PV)
	if $debug_cops;
    $copsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, %s, Nullhv, Nullgv, %u, %d, %u",
			  ${$op->next}, ${$op->sibling}, $op->ppaddr,
			  $op->targ, $op->type, $op_seq, $op->flags,
			  $op->private, cstring($op->label), $op->cop_seq,
			  $op->arybase, $op->line));
    my $copix = $copsect->index;
    $init->add(sprintf("cop_list[%d].cop_filegv = %s;", $copix, $gvsym),
	       sprintf("cop_list[%d].cop_stash = %s;", $copix, $stashsym));
    savesym($op, "(OP*)&cop_list[$copix]");
}

sub B::PMOP::save {
    my ($op, $level) = @_;
    my $replroot = $op->pmreplroot;
    my $replstart = $op->pmreplstart;
    my $replrootfield = sprintf("s\\_%x", $$replroot);
    my $replstartfield = sprintf("s\\_%x", $$replstart);
    my $gvsym;
    my $ppaddr = $op->ppaddr;
    if ($$replroot) {
	# OP_PUSHRE (a mutated version of OP_MATCH for the regexp
	# argument to a split) stores a GV in op_pmreplroot instead
	# of a substitution syntax tree. We don't want to walk that...
	if ($ppaddr eq "pp_pushre") {
	    $gvsym = $replroot->save;
#	    warn "PMOP::save saving a pp_pushre with GV $gvsym\n"; # debug
	    $replrootfield = 0;
	} else {
	    $replstartfield = saveoptree("*ignore*", $replroot, $replstart);
	}
    }
    # pmnext handling is broken in perl itself, I think. Bad op_pmnext
    # fields aren't noticed in perl's runtime (unless you try reset) but we
    # segfault when trying to dereference it to find op->op_pmnext->op_type
    $pmopsect->add(sprintf("s\\_%x, s\\_%x, %s, %u, %u, %u, 0x%x, 0x%x, s\\_%x, s\\_%x, %u, %s, %s, 0, 0, 0x%x, 0x%x",
			   ${$op->next}, ${$op->sibling}, $ppaddr, $op->targ,
			   $op->type, $op_seq, $op->flags, $op->private,
			   ${$op->first}, ${$op->last}, $op->children,
			   $replrootfield, $replstartfield,
			   $op->pmflags, $op->pmpermflags,));
    my $pm = sprintf("pmop_list[%d]", $pmopsect->index);
    my $re = $op->precomp;
    if (defined($re)) {
	my $resym = sprintf("re%d", $re_index++);
	$decl->add(sprintf("static char *$resym = %s;", cstring($re)));
	$init->add(sprintf("$pm.op_pmregexp = pregcomp($resym, $resym + %u, &$pm);",
			   length($re)));
    }
    if ($gvsym) {
	$init->add("$pm.op_pmreplroot = (OP*)$gvsym;");
    }
    savesym($op, sprintf("(OP*)&pmop_list[%d]", $pmopsect->index));
}

sub B::SPECIAL::save {
    my ($sv) = @_;
    # special case: $$sv is not the address but an index into specialsv_list
#   warn "SPECIAL::save specialsv $$sv\n"; # debug
    my $sym = $specialsv_name[$$sv];
    if (!defined($sym)) {
	confess "unknown specialsv index $$sv passed to B::SPECIAL::save";
    }
    return $sym;
}

sub B::OBJECT::save {}

sub B::NULL::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
#   warn "Saving SVt_NULL SV\n"; # debug
    # debug
    #if ($$sv == 0) {
    #	warn "NULL::save for sv = 0 called from @{[(caller(1))[3]]}\n";
    #}
    $svsect->add(sprintf("0, %u, 0x%x", $sv->REFCNT + 1, $sv->FLAGS));
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::IV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    $xpvivsect->add(sprintf("0, 0, 0, %d", $sv->IVX));
    $svsect->add(sprintf("&xpviv_list[%d], %lu, 0x%x",
			 $xpvivsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::NV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    $xpvnvsect->add(sprintf("0, 0, 0, %d, %s", $sv->IVX, $sv->NVX));
    $svsect->add(sprintf("&xpvnv_list[%d], %lu, 0x%x",
			 $xpvnvsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::PVLV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    my $pv = $sv->PV;
    my $len = length($pv);
    my ($pvsym, $pvmax) = savepv($pv);
    my ($lvtarg, $lvtarg_sym);
    $xpvlvsect->add(sprintf("%s, %u, %u, %d, %g, 0, 0, %u, %u, 0, %s",
			    $pvsym, $len, $pvmax, $sv->IVX, $sv->NVX, 
			    $sv->TARGOFF, $sv->TARGLEN, cchar($sv->TYPE)));
    $svsect->add(sprintf("&xpvlv_list[%d], %lu, 0x%x",
			 $xpvlvsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    if (!$pv_copy_on_grow) {
	$init->add(sprintf("xpvlv_list[%d].xpv_pv = savepvn(%s, %u);",
			   $xpvlvsect->index, cstring($pv), $len));
    }
    $sv->save_magic;
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::PVIV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    my $pv = $sv->PV;
    my $len = length($pv);
    my ($pvsym, $pvmax) = savepv($pv);
    $xpvivsect->add(sprintf("%s, %u, %u, %d", $pvsym, $len, $pvmax, $sv->IVX));
    $svsect->add(sprintf("&xpviv_list[%d], %u, 0x%x",
			 $xpvivsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    if (!$pv_copy_on_grow) {
	$init->add(sprintf("xpviv_list[%d].xpv_pv = savepvn(%s, %u);",
			   $xpvivsect->index, cstring($pv), $len));
    }
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::PVNV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    my $pv = $sv->PV;
    my $len = length($pv);
    my ($pvsym, $pvmax) = savepv($pv);
    $xpvnvsect->add(sprintf("%s, %u, %u, %d, %s",
			    $pvsym, $len, $pvmax, $sv->IVX, $sv->NVX));
    $svsect->add(sprintf("&xpvnv_list[%d], %lu, 0x%x",
			 $xpvnvsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    if (!$pv_copy_on_grow) {
	$init->add(sprintf("xpvnv_list[%d].xpv_pv = savepvn(%s,%u);",
			   $xpvnvsect->index, cstring($pv), $len));
    }
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::BM::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    my $pv = $sv->PV . "\0" . $sv->TABLE;
    my $len = length($pv);
    $xpvbmsect->add(sprintf("0, %u, %u, %d, %s, 0, 0, %d, %u, 0x%x",
			    $len, $len + 258, $sv->IVX, $sv->NVX,
			    $sv->USEFUL, $sv->PREVIOUS, $sv->RARE));
    $svsect->add(sprintf("&xpvbm_list[%d], %lu, 0x%x",
			 $xpvbmsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    $sv->save_magic;
    $init->add(sprintf("xpvbm_list[%d].xpv_pv = savepvn(%s, %u);",
		       $xpvbmsect->index, cstring($pv), $len),
	       sprintf("xpvbm_list[%d].xpv_cur = %u;",
		       $xpvbmsect->index, $len - 257));
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::PV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    my $pv = $sv->PV;
    my $len = length($pv);
    my ($pvsym, $pvmax) = savepv($pv);
    $xpvsect->add(sprintf("%s, %u, %u", $pvsym, $len, $pvmax));
    $svsect->add(sprintf("&xpv_list[%d], %lu, 0x%x",
			 $xpvsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    if (!$pv_copy_on_grow) {
	$init->add(sprintf("xpv_list[%d].xpv_pv = savepvn(%s, %u);",
			   $xpvsect->index, cstring($pv), $len));
    }
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub B::PVMG::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    my $pv = $sv->PV;
    my $len = length($pv);
    my ($pvsym, $pvmax) = savepv($pv);
    $xpvmgsect->add(sprintf("%s, %u, %u, %d, %s, 0, 0",
			    $pvsym, $len, $pvmax, $sv->IVX, $sv->NVX));
    $svsect->add(sprintf("&xpvmg_list[%d], %lu, 0x%x",
			 $xpvmgsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    if (!$pv_copy_on_grow) {
	$init->add(sprintf("xpvmg_list[%d].xpv_pv = savepvn(%s, %u);",
			   $xpvmgsect->index, cstring($pv), $len));
    }
    $sym = savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
    $sv->save_magic;
    return $sym;
}

sub B::PVMG::save_magic {
    my ($sv) = @_;
    #warn sprintf("saving magic for %s (0x%x)\n", class($sv), $$sv); # debug
    my $stash = $sv->SvSTASH;
    if ($$stash) {
	warn sprintf("xmg_stash = %s (0x%x)\n", $stash->NAME, $$stash)
	    if $debug_mg;
	# XXX Hope stash is already going to be saved.
	$init->add(sprintf("SvSTASH(s\\_%x) = s\\_%x;", $$sv, $$stash));
    }
    my @mgchain = $sv->MAGIC;
    my ($mg, $type, $obj, $ptr);
    foreach $mg (@mgchain) {
	$type = $mg->TYPE;
	$obj = $mg->OBJ;
	$ptr = $mg->PTR;
	my $len = defined($ptr) ? length($ptr) : 0;
	if ($debug_mg) {
	    warn sprintf("magic %s (0x%x), obj %s (0x%x), type %s, ptr %s\n",
			 class($sv), $$sv, class($obj), $$obj,
			 cchar($type), cstring($ptr));
	}
	$init->add(sprintf("sv_magic((SV*)s\\_%x, (SV*)s\\_%x, %s, %s, %d);",
			   $$sv, $$obj, cchar($type),cstring($ptr),$len));
    }
}

sub B::RV::save {
    my ($sv) = @_;
    my $sym = objsym($sv);
    return $sym if defined $sym;
    $xrvsect->add($sv->RV->save);
    $svsect->add(sprintf("&xrv_list[%d], %lu, 0x%x",
			 $xrvsect->index, $sv->REFCNT + 1, $sv->FLAGS));
    return savesym($sv, sprintf("&sv_list[%d]", $svsect->index));
}

sub try_autoload {
    my ($cvstashname, $cvname) = @_;
    warn sprintf("No definition for sub %s::%s\n", $cvstashname, $cvname);
    # Handle AutoLoader classes explicitly. Any more general AUTOLOAD
    # use should be handled by the class itself.
    no strict 'refs';
    my $isa = \@{"$cvstashname\::ISA"};
    if (grep($_ eq "AutoLoader", @$isa)) {
	warn "Forcing immediate load of sub derived from AutoLoader\n";
	# Tweaked version of AutoLoader::AUTOLOAD
	my $dir = $cvstashname;
	$dir =~ s(::)(/)g;
	eval { require "auto/$dir/$cvname.al" };
	if ($@) {
	    warn qq(failed require "auto/$dir/$cvname.al": $@\n);
	    return 0;
	} else {
	    return 1;
	}
    }
}

sub B::CV::save {
    my ($cv) = @_;
    my $sym = objsym($cv);
    if (defined($sym)) {
#	warn sprintf("CV 0x%x already saved as $sym\n", $$cv); # debug
	return $sym;
    }
    # Reserve a place in svsect and xpvcvsect and record indices
    my $sv_ix = $svsect->index + 1;
    $svsect->add("svix$sv_ix");
    my $xpvcv_ix = $xpvcvsect->index + 1;
    $xpvcvsect->add("xpvcvix$xpvcv_ix");
    # Save symbol now so that GvCV() doesn't recurse back to us via CvGV()
    $sym = savesym($cv, "&sv_list[$sv_ix]");
    warn sprintf("saving CV 0x%x as $sym\n", $$cv) if $debug_cv;
    my $gv = $cv->GV;
    my $cvstashname = $gv->STASH->NAME;
    my $cvname = $gv->NAME;
    my $root = $cv->ROOT;
    my $cvxsub = $cv->XSUB;
    if (!$$root && !$cvxsub) {
	if (try_autoload($cvstashname, $cvname)) {
	    # Recalculate root and xsub
	    $root = $cv->ROOT;
	    $cvxsub = $cv->XSUB;
	    if ($$root || $cvxsub) {
		warn "Successful forced autoload\n";
	    }
	}
    }
    my $startfield = 0;
    my $padlist = $cv->PADLIST;
    my $pv = $cv->PV;
    my $xsub = 0;
    my $xsubany = "Nullany";
    if ($$root) {
	warn sprintf("saving op tree for CV 0x%x, root = 0x%x\n",
		     $$cv, $$root) if $debug_cv;
	my $ppname = "";
	if ($$gv) {
	    my $stashname = $gv->STASH->NAME;
	    my $gvname = $gv->NAME;
	    if ($gvname ne "__ANON__") {
		$ppname = (${$gv->FORM} == $$cv) ? "pp_form_" : "pp_sub_";
		$ppname .= ($stashname eq "main") ?
			    $gvname : "$stashname\::$gvname";
		$ppname =~ s/::/__/g;
	    }
	}
	if (!$ppname) {
	    $ppname = "pp_anonsub_$anonsub_index";
	    $anonsub_index++;
	}
	$startfield = saveoptree($ppname, $root, $cv->START, $padlist->ARRAY);
	warn sprintf("done saving op tree for CV 0x%x, name %s, root 0x%x\n",
		     $$cv, $ppname, $$root) if $debug_cv;
	if ($$padlist) {
	    warn sprintf("saving PADLIST 0x%x for CV 0x%x\n",
			 $$padlist, $$cv) if $debug_cv;
	    $padlist->save;
	    warn sprintf("done saving PADLIST 0x%x for CV 0x%x\n",
			 $$padlist, $$cv) if $debug_cv;
	}
    }
    elsif ($cvxsub) {
	$xsubany = sprintf("ANYINIT((void*)0x%x)", $cv->XSUBANY);
	# Try to find out canonical name of XSUB function from EGV.
	# XXX Doesn't work for XSUBs with PREFIX set (or anyone who
	# calls newXS() manually with weird arguments).
	my $egv = $gv->EGV;
	my $stashname = $egv->STASH->NAME;
	$stashname =~ s/::/__/g;
	$xsub = sprintf("XS_%s_%s", $stashname, $egv->NAME);
	$decl->add("void $xsub _((CV*));");
    }
    else {
	warn sprintf("No definition for sub %s::%s (unable to autoload)\n",
		     $cvstashname, $cvname); # debug
    }
    $symsect->add(sprintf("xpvcvix%d\t%s, %u, 0, %d, %s, 0, Nullhv, Nullhv, %s, s\\_%x, $xsub, $xsubany, Nullgv, Nullgv, %d, s\\_%x, (CV*)s\\_%x, 0x%x",
			  $xpvcv_ix, cstring($pv), length($pv), $cv->IVX,
			  $cv->NVX, $startfield, ${$cv->ROOT}, $cv->DEPTH,
                        $$padlist, ${$cv->OUTSIDE}, $cv->CvFLAGS));

    if (${$cv->OUTSIDE} == ${main_cv()}){
	$init->add(sprintf("CvOUTSIDE(s\\_%x)=PL_main_cv;",$$cv));
    }

    if ($$gv) {
	$gv->save;
	$init->add(sprintf("CvGV(s\\_%x) = s\\_%x;",$$cv,$$gv));
	warn sprintf("done saving GV 0x%x for CV 0x%x\n",
		     $$gv, $$cv) if $debug_cv;
    }
    my $filegv = $cv->FILEGV;
    if ($$filegv) {
	$filegv->save;
	$init->add(sprintf("CvFILEGV(s\\_%x) = s\\_%x;", $$cv, $$filegv));
	warn sprintf("done saving FILEGV 0x%x for CV 0x%x\n",
		     $$filegv, $$cv) if $debug_cv;
    }
    my $stash = $cv->STASH;
    if ($$stash) {
	$stash->save;
	$init->add(sprintf("CvSTASH(s\\_%x) = s\\_%x;", $$cv, $$stash));
	warn sprintf("done saving STASH 0x%x for CV 0x%x\n",
		     $$stash, $$cv) if $debug_cv;
    }
    $symsect->add(sprintf("svix%d\t(XPVCV*)&xpvcv_list[%u], %lu, 0x%x",
			  $sv_ix, $xpvcv_ix, $cv->REFCNT + 1, $cv->FLAGS));
    return $sym;
}

sub B::GV::save {
    my ($gv) = @_;
    my $sym = objsym($gv);
    if (defined($sym)) {
	#warn sprintf("GV 0x%x already saved as $sym\n", $$gv); # debug
	return $sym;
    } else {
	my $ix = $gv_index++;
	$sym = savesym($gv, "gv_list[$ix]");
	#warn sprintf("Saving GV 0x%x as $sym\n", $$gv); # debug
    }
    my $gvname = $gv->NAME;
    my $name = cstring($gv->STASH->NAME . "::" . $gvname);
    #warn "GV name is $name\n"; # debug
    my $egv = $gv->EGV;
    my $egvsym;
    if ($$gv != $$egv) {
	#warn(sprintf("EGV name is %s, saving it now\n",
	#	     $egv->STASH->NAME . "::" . $egv->NAME)); # debug
	$egvsym = $egv->save;
    }
    $init->add(qq[$sym = gv_fetchpv($name, TRUE, SVt_PV);],
	       sprintf("SvFLAGS($sym) = 0x%x;", $gv->FLAGS),
	       sprintf("GvFLAGS($sym) = 0x%x;", $gv->GvFLAGS),
	       sprintf("GvLINE($sym) = %u;", $gv->LINE));
    # Shouldn't need to do save_magic since gv_fetchpv handles that
    #$gv->save_magic;
    my $refcnt = $gv->REFCNT + 1;
    $init->add(sprintf("SvREFCNT($sym) += %u;", $refcnt - 1)) if $refcnt > 1;
    my $gvrefcnt = $gv->GvREFCNT;
    if ($gvrefcnt > 1) {
	$init->add(sprintf("GvREFCNT($sym) += %u;", $gvrefcnt - 1));
    }
    if (defined($egvsym)) {
	# Shared glob *foo = *bar
	$init->add("gp_free($sym);",
		   "GvGP($sym) = GvGP($egvsym);");
    } elsif ($gvname !~ /^([^A-Za-z]|STDIN|STDOUT|STDERR|ARGV|SIG|ENV)$/) {
	# Don't save subfields of special GVs (*_, *1, *# and so on)
#	warn "GV::save saving subfields\n"; # debug
	my $gvsv = $gv->SV;
	if ($$gvsv) {
	    $init->add(sprintf("GvSV($sym) = s\\_%x;", $$gvsv));
#	    warn "GV::save \$$name\n"; # debug
	    $gvsv->save;
	}
	my $gvav = $gv->AV;
	if ($$gvav) {
	    $init->add(sprintf("GvAV($sym) = s\\_%x;", $$gvav));
#	    warn "GV::save \@$name\n"; # debug
	    $gvav->save;
	}
	my $gvhv = $gv->HV;
	if ($$gvhv) {
	    $init->add(sprintf("GvHV($sym) = s\\_%x;", $$gvhv));
#	    warn "GV::save \%$name\n"; # debug
	    $gvhv->save;
	}
	my $gvcv = $gv->CV;
	if ($$gvcv) {
	    $init->add(sprintf("GvCV($sym) = (CV*)s\\_%x;", $$gvcv));
#	    warn "GV::save &$name\n"; # debug
	    $gvcv->save;
	}
	my $gvfilegv = $gv->FILEGV;
	if ($$gvfilegv) {
	    $init->add(sprintf("GvFILEGV($sym) = (GV*)s\\_%x;",$$gvfilegv));
#	    warn "GV::save GvFILEGV(*$name)\n"; # debug
	    $gvfilegv->save;
	}
	my $gvform = $gv->FORM;
	if ($$gvform) {
	    $init->add(sprintf("GvFORM($sym) = (CV*)s\\_%x;", $$gvform));
#	    warn "GV::save GvFORM(*$name)\n"; # debug
	    $gvform->save;
	}
	my $gvio = $gv->IO;
	if ($$gvio) {
	    $init->add(sprintf("GvIOp($sym) = s\\_%x;", $$gvio));
#	    warn "GV::save GvIO(*$name)\n"; # debug
	    $gvio->save;
	}
    }
    return $sym;
}
sub B::AV::save {
    my ($av) = @_;
    my $sym = objsym($av);
    return $sym if defined $sym;
    my $avflags = $av->AvFLAGS;
    $xpvavsect->add(sprintf("0, -1, -1, 0, 0.0, 0, Nullhv, 0, 0, 0x%x",
			    $avflags));
    $svsect->add(sprintf("&xpvav_list[%d], %lu, 0x%x",
			 $xpvavsect->index, $av->REFCNT + 1, $av->FLAGS));
    my $sv_list_index = $svsect->index;
    my $fill = $av->FILL;
    $av->save_magic;
    warn sprintf("saving AV 0x%x FILL=$fill AvFLAGS=0x%x", $$av, $avflags)
	if $debug_av;
    # XXX AVf_REAL is wrong test: need to save comppadlist but not stack
    #if ($fill > -1 && ($avflags & AVf_REAL)) {
    if ($fill > -1) {
	my @array = $av->ARRAY;
	if ($debug_av) {
	    my $el;
	    my $i = 0;
	    foreach $el (@array) {
		warn sprintf("AV 0x%x[%d] = %s 0x%x\n",
			     $$av, $i++, class($el), $$el);
	    }
	}
	my @names = map($_->save, @array);
	# XXX Better ways to write loop?
	# Perhaps svp[0] = ...; svp[1] = ...; svp[2] = ...;
	# Perhaps I32 i = 0; svp[i++] = ...; svp[i++] = ...; svp[i++] = ...;
	$init->add("{",
		   "\tSV **svp;",
		   "\tAV *av = (AV*)&sv_list[$sv_list_index];",
		   "\tav_extend(av, $fill);",
		   "\tsvp = AvARRAY(av);",
	       map("\t*svp++ = (SV*)$_;", @names),
		   "\tAvFILLp(av) = $fill;",
		   "}");
    } else {
	my $max = $av->MAX;
	$init->add("av_extend((AV*)&sv_list[$sv_list_index], $max);")
	    if $max > -1;
    }
    return savesym($av, "(AV*)&sv_list[$sv_list_index]");
}

sub B::HV::save {
    my ($hv) = @_;
    my $sym = objsym($hv);
    return $sym if defined $sym;
    my $name = $hv->NAME;
    if ($name) {
	# It's a stash

	# A perl bug means HvPMROOT isn't altered when a PMOP is freed. Usually
	# the only symptom is that sv_reset tries to reset the PMf_USED flag of
	# a trashed op but we look at the trashed op_type and segfault.
	#my $adpmroot = ${$hv->PMROOT};
	my $adpmroot = 0;
	$decl->add("static HV *hv$hv_index;");
	# XXX Beware of weird package names containing double-quotes, \n, ...?
	$init->add(qq[hv$hv_index = gv_stashpv("$name", TRUE);]);
	if ($adpmroot) {
	    $init->add(sprintf("HvPMROOT(hv$hv_index) = (PMOP*)s\\_%x;",
			       $adpmroot));
	}
	$sym = savesym($hv, "hv$hv_index");
	$hv_index++;
	return $sym;
    }
    # It's just an ordinary HV
    $xpvhvsect->add(sprintf("0, 0, %d, 0, 0.0, 0, Nullhv, %d, 0, 0, 0",
			    $hv->MAX, $hv->RITER));
    $svsect->add(sprintf("&xpvhv_list[%d], %lu, 0x%x",
			 $xpvhvsect->index, $hv->REFCNT + 1, $hv->FLAGS));
    my $sv_list_index = $svsect->index;
    my @contents = $hv->ARRAY;
    if (@contents) {
	my $i;
	for ($i = 1; $i < @contents; $i += 2) {
	    $contents[$i] = $contents[$i]->save;
	}
	$init->add("{", "\tHV *hv = (HV*)&sv_list[$sv_list_index];");
	while (@contents) {
	    my ($key, $value) = splice(@contents, 0, 2);
	    $init->add(sprintf("\thv_store(hv, %s, %u, %s, %s);",
			       cstring($key),length($key),$value, hash($key)));
	}
	$init->add("}");
    }
    return savesym($hv, "(HV*)&sv_list[$sv_list_index]");
}

sub B::IO::save {
    my ($io) = @_;
    my $sym = objsym($io);
    return $sym if defined $sym;
    my $pv = $io->PV;
    my $len = length($pv);
    $xpviosect->add(sprintf("0, %u, %u, %d, %s, 0, 0, 0, 0, 0, %d, %d, %d, %d, %s, Nullgv, %s, Nullgv, %s, Nullgv, %d, %s, 0x%x",
			    $len, $len+1, $io->IVX, $io->NVX, $io->LINES,
			    $io->PAGE, $io->PAGE_LEN, $io->LINES_LEFT,
			    cstring($io->TOP_NAME), cstring($io->FMT_NAME), 
			    cstring($io->BOTTOM_NAME), $io->SUBPROCESS,
			    cchar($io->IoTYPE), $io->IoFLAGS));
    $svsect->add(sprintf("&xpvio_list[%d], %lu, 0x%x",
			 $xpviosect->index, $io->REFCNT + 1, $io->FLAGS));
    $sym = savesym($io, sprintf("(IO*)&sv_list[%d]", $svsect->index));
    my ($field, $fsym);
    foreach $field (qw(TOP_GV FMT_GV BOTTOM_GV)) {
      	$fsym = $io->$field();
	if ($$fsym) {
	    $init->add(sprintf("Io$field($sym) = (GV*)s\\_%x;", $$fsym));
	    $fsym->save;
	}
    }
    $io->save_magic;
    return $sym;
}

sub B::SV::save {
    my $sv = shift;
    # This is where we catch an honest-to-goodness Nullsv (which gets
    # blessed into B::SV explicitly) and any stray erroneous SVs.
    return 0 unless $$sv;
    confess sprintf("cannot save that type of SV: %s (0x%x)\n",
		    class($sv), $$sv);
}

sub output_all {
    my $init_name = shift;
    my $section;
    my @sections = ($opsect, $unopsect, $binopsect, $logopsect, $condopsect,
		    $listopsect, $pmopsect, $svopsect, $gvopsect, $pvopsect,
		    $cvopsect, $loopsect, $copsect, $svsect, $xpvsect,
		    $xpvavsect, $xpvhvsect, $xpvcvsect, $xpvivsect, $xpvnvsect,
		    $xpvmgsect, $xpvlvsect, $xrvsect, $xpvbmsect, $xpviosect);
    $bootstrap->output(\*STDOUT, "/* bootstrap %s */\n");
    $symsect->output(\*STDOUT, "#define %s\n");
    print "\n";
    output_declarations();
    foreach $section (@sections) {
	my $lines = $section->index + 1;
	if ($lines) {
	    my $name = $section->name;
	    my $typename = ($name eq "xpvcv") ? "XPVCV_or_similar" : uc($name);
	    print "Static $typename ${name}_list[$lines];\n";
	}
    }
    $decl->output(\*STDOUT, "%s\n");
    print "\n";
    foreach $section (@sections) {
	my $lines = $section->index + 1;
	if ($lines) {
	    my $name = $section->name;
	    my $typename = ($name eq "xpvcv") ? "XPVCV_or_similar" : uc($name);
	    printf "static %s %s_list[%u] = {\n", $typename, $name, $lines;
	    $section->output(\*STDOUT, "\t{ %s },\n");
	    print "};\n\n";
	}
    }

    print <<"EOT";
static int $init_name()
{
	dTHR;
EOT
    $init->output(\*STDOUT, "\t%s\n");
    print "\treturn 0;\n}\n";
    if ($verbose) {
	warn compile_stats();
	warn "NULLOP count: $nullop_count\n";
    }
}

sub output_declarations {
    print <<'EOT';
#ifdef BROKEN_STATIC_REDECL
#define Static extern
#else
#define Static static
#endif /* BROKEN_STATIC_REDECL */

#ifdef BROKEN_UNION_INIT
/*
 * Cribbed from cv.h with ANY (a union) replaced by void*.
 * Some pre-Standard compilers can't cope with initialising unions. Ho hum.
 */
typedef struct {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xp_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xof_off;	/* integer value */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    HV *	xcv_stash;
    OP *	xcv_start;
    OP *	xcv_root;
    void      (*xcv_xsub) _((CV*));
    void *	xcv_xsubany;
    GV *	xcv_gv;
    GV *	xcv_filegv;
    long	xcv_depth;		/* >= 2 indicates recursive call */
    AV *	xcv_padlist;
    CV *	xcv_outside;
#ifdef USE_THREADS
    perl_mutex *xcv_mutexp;
    struct perl_thread *xcv_owner;	/* current owner thread */
#endif /* USE_THREADS */
    U8		xcv_flags;
} XPVCV_or_similar;
#define ANYINIT(i) i
#else
#define XPVCV_or_similar XPVCV
#define ANYINIT(i) {i}
#endif /* BROKEN_UNION_INIT */
#define Nullany ANYINIT(0)

#define UNUSED 0
#define sym_0 0

EOT
    print "static GV *gv_list[$gv_index];\n" if $gv_index;
    print "\n";
}


sub output_boilerplate {
    print <<'EOT';
#include "EXTERN.h"
#include "perl.h"
#ifndef PATCHLEVEL
#include "patchlevel.h"
#endif

/* Workaround for mapstart: the only op which needs a different ppaddr */
#undef pp_mapstart
#define pp_mapstart pp_grepstart

static void xs_init _((void));
static PerlInterpreter *my_perl;
EOT
}

sub output_main {
    print <<'EOT';
int
#ifndef CAN_PROTOTYPE
main(argc, argv, env)
int argc;
char **argv;
char **env;
#else  /* def(CAN_PROTOTYPE) */
main(int argc, char **argv, char **env)
#endif  /* def(CAN_PROTOTYPE) */
{
    int exitstatus;
    int i;
    char **fakeargv;

    PERL_SYS_INIT(&argc,&argv);
 
    perl_init_i18nl10n(1);

    if (!PL_do_undump) {
	my_perl = perl_alloc();
	if (!my_perl)
	    exit(1);
	perl_construct( my_perl );
    }

#ifdef CSH
    if (!PL_cshlen) 
      PL_cshlen = strlen(PL_cshname);
#endif

#ifdef ALLOW_PERL_OPTIONS
#define EXTRA_OPTIONS 2
#else
#define EXTRA_OPTIONS 3
#endif /* ALLOW_PERL_OPTIONS */
    New(666, fakeargv, argc + EXTRA_OPTIONS + 1, char *);
    fakeargv[0] = argv[0];
    fakeargv[1] = "-e";
    fakeargv[2] = "";
#ifndef ALLOW_PERL_OPTIONS
    fakeargv[3] = "--";
#endif /* ALLOW_PERL_OPTIONS */
    for (i = 1; i < argc; i++)
	fakeargv[i + EXTRA_OPTIONS] = argv[i];
    fakeargv[argc + EXTRA_OPTIONS] = 0;
    
    exitstatus = perl_parse(my_perl, xs_init, argc + EXTRA_OPTIONS,
			    fakeargv, NULL);
    if (exitstatus)
	exit( exitstatus );

    sv_setpv(GvSV(gv_fetchpv("0", TRUE, SVt_PV)), argv[0]);
    PL_main_cv = PL_compcv;
    PL_compcv = 0;

    exitstatus = perl_init();
    if (exitstatus)
	exit( exitstatus );

    exitstatus = perl_run( my_perl );

    perl_destruct( my_perl );
    perl_free( my_perl );

    exit( exitstatus );
}

static void
xs_init()
{
}
EOT
}

sub dump_symtable {
    # For debugging
    my ($sym, $val);
    warn "----Symbol table:\n";
    while (($sym, $val) = each %symtable) {
	warn "$sym => $val\n";
    }
    warn "---End of symbol table\n";
}

sub save_object {
    my $sv;
    foreach $sv (@_) {
	svref_2object($sv)->save;
    }
}       

sub Dummy_BootStrap { }            

sub B::GV::savecv {
    my $gv = shift;
    my $cv = $gv->CV;
    my $name = $gv->NAME;
    if ($$cv) {
	if ($name eq "bootstrap" && $cv->XSUB) {
	    my $file = $cv->FILEGV->SV->PV;
	    $bootstrap->add($file);
	    my $name = $gv->STASH->NAME.'::'.$name;
	    no strict 'refs';
            *{$name} = \&Dummy_BootStrap;   
	    $cv = $gv->CV;
	}
	if ($debug_cv) {
	    warn sprintf("saving extra CV &%s::%s (0x%x) from GV 0x%x\n",
			 $gv->STASH->NAME, $name, $$cv, $$gv);
	}
      my $package=$gv->STASH->NAME;
      # This seems to undo all the ->isa and prefix stuff we do below
      # so disable again for now
      if (0 && ! grep(/^$package$/,@unused_sub_packages)){
          warn sprintf("omitting cv in superclass %s", $gv->STASH->NAME) 
              if $debug_cv;
          return ;
      }
	$gv->save;
    }
    elsif ($name eq 'ISA')
     {
      $gv->save;
     }

}



sub save_unused_subs {
    my %search_pack;
    map { $search_pack{$_} = 1 } @_;
    @unused_sub_packages=@_;
    no strict qw(vars refs);
    walksymtable(\%{"main::"}, "savecv", sub {
	my $package = shift;
	$package =~ s/::$//;
	return 0 if ($package =~ /::::/);  # skip ::::ISA::CACHE etc.
	#warn "Considering $package\n";#debug
	return 1 if exists $search_pack{$package};
      #sub try for a partial match
      if (grep(/^$package\:\:/,@unused_sub_packages)){ 
          return 1;   
      }       
	#warn "    (nothing explicit)\n";#debug
	# Omit the packages which we use (and which cause grief
	# because of fancy "goto &$AUTOLOAD" stuff).
	# XXX Surely there must be a nicer way to do this.
	if ($package eq "FileHandle"
	    || $package eq "Config"
	    || $package eq "SelectSaver") {
	    return 0;
	}
	foreach my $u (keys %search_pack) {
	    if ($package =~ /^${u}::/) {
		warn "$package starts with $u\n";
		return 1
	    }
	    if ($package->isa($u)) {
		warn "$package isa $u\n";
		return 1
	    }
	    return 1 if $package->isa($u);
	}
	foreach my $m (qw(new DESTROY TIESCALAR TIEARRAY TIEHASH)) {
	    if (defined(&{$package."::$m"})) {
		warn "$package has method $m: -u$package assumed\n";#debug
              push @unused_sub_package, $package;
		return 1;
	    }
	}
	return 0;
    });
}

sub save_main {
    warn "Walking tree\n";
    my $curpad_nam = (comppadlist->ARRAY)[0]->save;
    my $curpad_sym = (comppadlist->ARRAY)[1]->save;
    my $init_av    = init_av->save;
    my $inc_hv     = svref_2object(\%INC)->save;
    my $inc_av     = svref_2object(\@INC)->save;
    walkoptree(main_root, "save");
    warn "done main optree, walking symtable for extras\n" if $debug_cv;
    save_unused_subs(@unused_sub_packages);

    $init->add(sprintf("PL_main_root = s\\_%x;", ${main_root()}),
	       sprintf("PL_main_start = s\\_%x;", ${main_start()}),
	       "PL_curpad = AvARRAY($curpad_sym);",
	       "PL_initav = $init_av;",
	       "GvHV(PL_incgv) = $inc_hv;",
	       "GvAV(PL_incgv) = $inc_av;",
               "av_store(CvPADLIST(PL_main_cv),0,SvREFCNT_inc($curpad_nam));",
               "av_store(CvPADLIST(PL_main_cv),1,SvREFCNT_inc($curpad_sym));");
    warn "Writing output\n";
    output_boilerplate();
    print "\n";
    output_all("perl_init");
    print "\n";
    output_main();
}

sub init_sections {
    my @sections = (init => \$init, decl => \$decl, sym => \$symsect,
		    binop => \$binopsect, condop => \$condopsect,
		    cop => \$copsect, cvop => \$cvopsect, gvop => \$gvopsect,
		    listop => \$listopsect, logop => \$logopsect,
		    loop => \$loopsect, op => \$opsect, pmop => \$pmopsect,
		    pvop => \$pvopsect, svop => \$svopsect, unop => \$unopsect,
		    sv => \$svsect, xpv => \$xpvsect, xpvav => \$xpvavsect,
		    xpvhv => \$xpvhvsect, xpvcv => \$xpvcvsect,
		    xpviv => \$xpvivsect, xpvnv => \$xpvnvsect,
		    xpvmg => \$xpvmgsect, xpvlv => \$xpvlvsect,
		    xrv => \$xrvsect, xpvbm => \$xpvbmsect,
		    xpvio => \$xpviosect, bootstrap => \$bootstrap);
    my ($name, $sectref);
    while (($name, $sectref) = splice(@sections, 0, 2)) {
	$$sectref = new B::Section $name, \%symtable, 0;
    }
}

sub compile {
    my @options = @_;
    my ($option, $opt, $arg);
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
	}
	if ($opt eq "w") {
	    $warn_undefined_syms = 1;
	} elsif ($opt eq "D") {
	    $arg ||= shift @options;
	    foreach $arg (split(//, $arg)) {
		if ($arg eq "o") {
		    B->debug(1);
		} elsif ($arg eq "c") {
		    $debug_cops = 1;
		} elsif ($arg eq "A") {
		    $debug_av = 1;
		} elsif ($arg eq "C") {
		    $debug_cv = 1;
		} elsif ($arg eq "M") {
		    $debug_mg = 1;
		} else {
		    warn "ignoring unknown debug option: $arg\n";
		}
	    }
	} elsif ($opt eq "o") {
	    $arg ||= shift @options;
	    open(STDOUT, ">$arg") or return "$arg: $!\n";
	} elsif ($opt eq "v") {
	    $verbose = 1;
	} elsif ($opt eq "u") {
	    $arg ||= shift @options;
	    push(@unused_sub_packages, $arg);
	} elsif ($opt eq "f") {
	    $arg ||= shift @options;
	    if ($arg eq "cog") {
		$pv_copy_on_grow = 1;
	    } elsif ($arg eq "no-cog") {
		$pv_copy_on_grow = 0;
	    }
	} elsif ($opt eq "O") {
	    $arg = 1 if $arg eq "";
	    $pv_copy_on_grow = 0;
	    if ($arg >= 1) {
		# Optimisations for -O1
		$pv_copy_on_grow = 1;
	    }
	}
    }
    init_sections();
    if (@options) {
	return sub {
	    my $objname;
	    foreach $objname (@options) {
		eval "save_object(\\$objname)";
	    }
	    output_all();
	}
    } else {
	return sub { save_main() };
    }
}

1;

__END__

=head1 NAME

B::C - Perl compiler's C backend

=head1 SYNOPSIS

	perl -MO=C[,OPTIONS] foo.pl

=head1 DESCRIPTION

This compiler backend takes Perl source and generates C source code
corresponding to the internal structures that perl uses to run
your program. When the generated C source is compiled and run, it
cuts out the time which perl would have taken to load and parse
your program into its internal semi-compiled form. That means that
compiling with this backend will not help improve the runtime
execution speed of your program but may improve the start-up time.
Depending on the environment in which your program runs this may be
either a help or a hindrance.

=head1 OPTIONS

If there are any non-option arguments, they are taken to be
names of objects to be saved (probably doesn't work properly yet).
Without extra arguments, it saves the main program.

=over 4

=item B<-ofilename>

Output to filename instead of STDOUT

=item B<-v>

Verbose compilation (currently gives a few compilation statistics).

=item B<-->

Force end of options

=item B<-uPackname>

Force apparently unused subs from package Packname to be compiled.
This allows programs to use eval "foo()" even when sub foo is never
seen to be used at compile time. The down side is that any subs which
really are never used also have code generated. This option is
necessary, for example, if you have a signal handler foo which you
initialise with C<$SIG{BAR} = "foo">.  A better fix, though, is just
to change it to C<$SIG{BAR} = \&foo>. You can have multiple B<-u>
options. The compiler tries to figure out which packages may possibly
have subs in which need compiling but the current version doesn't do
it very well. In particular, it is confused by nested packages (i.e.
of the form C<A::B>) where package C<A> does not contain any subs.

=item B<-D>

Debug options (concatenated or separate flags like C<perl -D>).

=item B<-Do>

OPs, prints each OP as it's processed

=item B<-Dc>

COPs, prints COPs as processed (incl. file & line num)

=item B<-DA>

prints AV information on saving

=item B<-DC>

prints CV information on saving

=item B<-DM>

prints MAGIC information on saving

=item B<-f>

Force optimisations on or off one at a time.

=item B<-fcog>

Copy-on-grow: PVs declared and initialised statically.

=item B<-fno-cog>

No copy-on-grow.

=item B<-On>

Optimisation level (n = 0, 1, 2, ...). B<-O> means B<-O1>.  Currently,
B<-O1> and higher set B<-fcog>.

=head1 EXAMPLES

    perl -MO=C,-ofoo.c foo.pl
    perl cc_harness -o foo foo.c

Note that C<cc_harness> lives in the C<B> subdirectory of your perl
library directory. The utility called C<perlcc> may also be used to
help make use of this compiler.

    perl -MO=C,-v,-DcA bar.pl > /dev/null

=head1 BUGS

Plenty. Current status: experimental.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
