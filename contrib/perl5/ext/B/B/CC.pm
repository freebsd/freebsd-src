#      CC.pm
#
#      Copyright (c) 1996, 1997, 1998 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
#
package B::CC;
use Config;
use strict;
use B qw(main_start main_root class comppadlist peekop svref_2object
	timing_info init_av sv_undef amagic_generation 
	OPf_WANT_LIST OPf_WANT OPf_MOD OPf_STACKED OPf_SPECIAL
	OPpASSIGN_BACKWARDS OPpLVAL_INTRO OPpDEREF_AV OPpDEREF_HV
	OPpDEREF OPpFLIP_LINENUM G_ARRAY G_SCALAR    
	CXt_NULL CXt_SUB CXt_EVAL CXt_LOOP CXt_SUBST CXt_BLOCK
	);
use B::C qw(save_unused_subs objsym init_sections mark_unused
	    output_all output_boilerplate output_main);
use B::Bblock qw(find_leaders);
use B::Stackobj qw(:types :flags);

# These should probably be elsewhere
# Flags for $op->flags

my $module;		# module name (when compiled with -m)
my %done;		# hash keyed by $$op of leaders of basic blocks
			# which have already been done.
my $leaders;		# ref to hash of basic block leaders. Keys are $$op
			# addresses, values are the $op objects themselves.
my @bblock_todo;	# list of leaders of basic blocks that need visiting
			# sometime.
my @cc_todo;		# list of tuples defining what PP code needs to be
			# saved (e.g. CV, main or PMOP repl code). Each tuple
			# is [$name, $root, $start, @padlist]. PMOP repl code
			# tuples inherit padlist.
my @stack;		# shadows perl's stack when contents are known.
			# Values are objects derived from class B::Stackobj
my @pad;		# Lexicals in current pad as Stackobj-derived objects
my @padlist;		# Copy of current padlist so PMOP repl code can find it
my @cxstack;		# Shadows the (compile-time) cxstack for next,last,redo
my $jmpbuf_ix = 0;	# Next free index for dynamically allocated jmpbufs
my %constobj;		# OP_CONST constants as Stackobj-derived objects
			# keyed by $$sv.
my $need_freetmps = 0;	# We may postpone FREETMPS to the end of each basic
			# block or even to the end of each loop of blocks,
			# depending on optimisation options.
my $know_op = 0;	# Set when C variable op already holds the right op
			# (from an immediately preceding DOOP(ppname)).
my $errors = 0;		# Number of errors encountered
my %skip_stack;		# Hash of PP names which don't need write_back_stack
my %skip_lexicals;	# Hash of PP names which don't need write_back_lexicals
my %skip_invalidate;	# Hash of PP names which don't need invalidate_lexicals
my %ignore_op;		# Hash of ops which do nothing except returning op_next
my %need_curcop;	# Hash of ops which need PL_curcop

my %lexstate;		#state of padsvs at the start of a bblock

BEGIN {
    foreach (qw(pp_scalar pp_regcmaybe pp_lineseq pp_scope pp_null)) {
	$ignore_op{$_} = 1;
    }
}

my ($module_name);
my ($debug_op, $debug_stack, $debug_cxstack, $debug_pad, $debug_runtime,
    $debug_shadow, $debug_queue, $debug_lineno, $debug_timings);

# Optimisation options. On the command line, use hyphens instead of
# underscores for compatibility with gcc-style options. We use
# underscores here because they are OK in (strict) barewords.
my ($freetmps_each_bblock, $freetmps_each_loop, $omit_taint);
my %optimise = (freetmps_each_bblock	=> \$freetmps_each_bblock,
		freetmps_each_loop	=> \$freetmps_each_loop,
		omit_taint		=> \$omit_taint);
# perl patchlevel to generate code for (defaults to current patchlevel)
my $patchlevel = int(0.5 + 1000 * ($]  - 5));

# Could rewrite push_runtime() and output_runtime() to use a
# temporary file if memory is at a premium.
my $ppname;		# name of current fake PP function
my $runtime_list_ref;
my $declare_ref;	# Hash ref keyed by C variable type of declarations.

my @pp_list;		# list of [$ppname, $runtime_list_ref, $declare_ref]
			# tuples to be written out.

my ($init, $decl);

sub init_hash { map { $_ => 1 } @_ }

#
# Initialise the hashes for the default PP functions where we can avoid
# either write_back_stack, write_back_lexicals or invalidate_lexicals.
#
%skip_lexicals = init_hash qw(pp_enter pp_enterloop);
%skip_invalidate = init_hash qw(pp_enter pp_enterloop);
%need_curcop = init_hash qw(pp_rv2gv  pp_bless pp_repeat pp_sort pp_caller
			pp_reset pp_rv2cv pp_entereval pp_require pp_dofile
			pp_entertry pp_enterloop pp_enteriter pp_entersub
			pp_enter pp_method);

sub debug {
    if ($debug_runtime) {
	warn(@_);
    } else {
	my @tmp=@_;
	runtime(map { chomp; "/* $_ */"} @tmp);
    }
}

sub declare {
    my ($type, $var) = @_;
    push(@{$declare_ref->{$type}}, $var);
}

sub push_runtime {
    push(@$runtime_list_ref, @_);
    warn join("\n", @_) . "\n" if $debug_runtime;
}

sub save_runtime {
    push(@pp_list, [$ppname, $runtime_list_ref, $declare_ref]);
}

sub output_runtime {
    my $ppdata;
    print qq(#include "cc_runtime.h"\n);
    foreach $ppdata (@pp_list) {
	my ($name, $runtime, $declare) = @$ppdata;
	print "\nstatic\nCCPP($name)\n{\n";
	my ($type, $varlist, $line);
	while (($type, $varlist) = each %$declare) {
	    print "\t$type ", join(", ", @$varlist), ";\n";
	}
	foreach $line (@$runtime) {
	    print $line, "\n";
	}
	print "}\n";
    }
}

sub runtime {
    my $line;
    foreach $line (@_) {
	push_runtime("\t$line");
    }
}

sub init_pp {
    $ppname = shift;
    $runtime_list_ref = [];
    $declare_ref = {};
    runtime("dSP;");
    declare("I32", "oldsave");
    declare("SV", "**svp");
    map { declare("SV", "*$_") } qw(sv src dst left right);
    declare("MAGIC", "*mg");
    $decl->add("static OP * $ppname (pTHX);");
    debug "init_pp: $ppname\n" if $debug_queue;
}

# Initialise runtime_callback function for Stackobj class
BEGIN { B::Stackobj::set_callback(\&runtime) }

# Initialise saveoptree_callback for B::C class
sub cc_queue {
    my ($name, $root, $start, @pl) = @_;
    debug "cc_queue: name $name, root $root, start $start, padlist (@pl)\n"
	if $debug_queue;
    if ($name eq "*ignore*") {
	$name = 0;
    } else {
	push(@cc_todo, [$name, $root, $start, (@pl ? @pl : @padlist)]);
    }
    my $fakeop = new B::FAKEOP ("next" => 0, sibling => 0, ppaddr => $name);
    $start = $fakeop->save;
    debug "cc_queue: name $name returns $start\n" if $debug_queue;
    return $start;
}
BEGIN { B::C::set_callback(\&cc_queue) }

sub valid_int { $_[0]->{flags} & VALID_INT }
sub valid_double { $_[0]->{flags} & VALID_DOUBLE }
sub valid_numeric { $_[0]->{flags} & (VALID_INT | VALID_DOUBLE) }
sub valid_sv { $_[0]->{flags} & VALID_SV }

sub top_int { @stack ? $stack[-1]->as_int : "TOPi" }
sub top_double { @stack ? $stack[-1]->as_double : "TOPn" }
sub top_numeric { @stack ? $stack[-1]->as_numeric : "TOPn" }
sub top_sv { @stack ? $stack[-1]->as_sv : "TOPs" }
sub top_bool { @stack ? $stack[-1]->as_bool : "SvTRUE(TOPs)" }

sub pop_int { @stack ? (pop @stack)->as_int : "POPi" }
sub pop_double { @stack ? (pop @stack)->as_double : "POPn" }
sub pop_numeric { @stack ? (pop @stack)->as_numeric : "POPn" }
sub pop_sv { @stack ? (pop @stack)->as_sv : "POPs" }
sub pop_bool {
    if (@stack) {
	return ((pop @stack)->as_bool);
    } else {
	# Careful: POPs has an auto-decrement and SvTRUE evaluates
	# its argument more than once.
	runtime("sv = POPs;");
	return "SvTRUE(sv)";
    }
}

sub write_back_lexicals {
    my $avoid = shift || 0;
    debug "write_back_lexicals($avoid) called from @{[(caller(1))[3]]}\n"
	if $debug_shadow;
    my $lex;
    foreach $lex (@pad) {
	next unless ref($lex);
	$lex->write_back unless $lex->{flags} & $avoid;
    }
}

sub save_or_restore_lexical_state {
    my $bblock=shift;
    unless( exists $lexstate{$bblock}){
    	foreach my $lex (@pad) {
		next unless ref($lex);
		${$lexstate{$bblock}}{$lex->{iv}} = $lex->{flags} ;
	}
    }
    else {
    	foreach my $lex (@pad) {
	    next unless ref($lex);
	    my $old_flags=${$lexstate{$bblock}}{$lex->{iv}}  ;
	    next if ( $old_flags eq $lex->{flags});
	    if  (($old_flags & VALID_SV)  && !($lex->{flags} & VALID_SV)){
		$lex->write_back;
	    }
	    if (($old_flags & VALID_DOUBLE) && !($lex->{flags} & VALID_DOUBLE)){
                $lex->load_double;
            }
            if (($old_flags & VALID_INT) && !($lex->{flags} & VALID_INT)){
                $lex->load_int;
            }
        }
    }
}

sub write_back_stack {
    my $obj;
    return unless @stack;
    runtime(sprintf("EXTEND(sp, %d);", scalar(@stack)));
    foreach $obj (@stack) {
	runtime(sprintf("PUSHs((SV*)%s);", $obj->as_sv));
    }
    @stack = ();
}

sub invalidate_lexicals {
    my $avoid = shift || 0;
    debug "invalidate_lexicals($avoid) called from @{[(caller(1))[3]]}\n"
	if $debug_shadow;
    my $lex;
    foreach $lex (@pad) {
	next unless ref($lex);
	$lex->invalidate unless $lex->{flags} & $avoid;
    }
}

sub reload_lexicals {
    my $lex;
    foreach $lex (@pad) {
	next unless ref($lex);
	my $type = $lex->{type};
	if ($type == T_INT) {
	    $lex->as_int;
	} elsif ($type == T_DOUBLE) {
	    $lex->as_double;
	} else {
	    $lex->as_sv;
	}
    }
}

{
    package B::Pseudoreg;
    #
    # This class allocates pseudo-registers (OK, so they're C variables).
    #
    my %alloc;		# Keyed by variable name. A value of 1 means the
			# variable has been declared. A value of 2 means
			# it's in use.
    
    sub new_scope { %alloc = () }
    
    sub new ($$$) {
	my ($class, $type, $prefix) = @_;
	my ($ptr, $i, $varname, $status, $obj);
	$prefix =~ s/^(\**)//;
	$ptr = $1;
	$i = 0;
	do {
	    $varname = "$prefix$i";
	    $status = $alloc{$varname};
	} while $status == 2;
	if ($status != 1) {
	    # Not declared yet
	    B::CC::declare($type, "$ptr$varname");
	    $alloc{$varname} = 2;	# declared and in use
	}
	$obj = bless \$varname, $class;
	return $obj;
    }
    sub DESTROY {
	my $obj = shift;
	$alloc{$$obj} = 1; # no longer in use but still declared
    }
}
{
    package B::Shadow;
    #
    # This class gives a standard API for a perl object to shadow a
    # C variable and only generate reloads/write-backs when necessary.
    #
    # Use $obj->load($foo) instead of runtime("shadowed_c_var = foo").
    # Use $obj->write_back whenever shadowed_c_var needs to be up to date.
    # Use $obj->invalidate whenever an unknown function may have
    # set shadow itself.

    sub new {
	my ($class, $write_back) = @_;
	# Object fields are perl shadow variable, validity flag
	# (for *C* variable) and callback sub for write_back
	# (passed perl shadow variable as argument).
	bless [undef, 1, $write_back], $class;
    }
    sub load {
	my ($obj, $newval) = @_;
	$obj->[1] = 0;		# C variable no longer valid
	$obj->[0] = $newval;
    }
    sub write_back {
	my $obj = shift;
	if (!($obj->[1])) {
	    $obj->[1] = 1;	# C variable will now be valid
	    &{$obj->[2]}($obj->[0]);
	}
    }
    sub invalidate { $_[0]->[1] = 0 } # force C variable to be invalid
}
my $curcop = new B::Shadow (sub {
    my $opsym = shift->save;
    runtime("PL_curcop = (COP*)$opsym;");
});

#
# Context stack shadowing. Mimics stuff in pp_ctl.c, cop.h and so on.
#
sub dopoptoloop {
    my $cxix = $#cxstack;
    while ($cxix >= 0 && $cxstack[$cxix]->{type} != CXt_LOOP) {
	$cxix--;
    }
    debug "dopoptoloop: returning $cxix" if $debug_cxstack;
    return $cxix;
}

sub dopoptolabel {
    my $label = shift;
    my $cxix = $#cxstack;
    while ($cxix >= 0 &&
	   ($cxstack[$cxix]->{type} != CXt_LOOP ||
	    $cxstack[$cxix]->{label} ne $label)) {
	$cxix--;
    }
    debug "dopoptolabel: returning $cxix" if $debug_cxstack;
    return $cxix;
}

sub error {
    my $format = shift;
    my $file = $curcop->[0]->file;
    my $line = $curcop->[0]->line;
    $errors++;
    if (@_) {
	warn sprintf("%s:%d: $format\n", $file, $line, @_);
    } else {
	warn sprintf("%s:%d: %s\n", $file, $line, $format);
    }
}

#
# Load pad takes (the elements of) a PADLIST as arguments and loads
# up @pad with Stackobj-derived objects which represent those lexicals.
# If/when perl itself can generate type information (my int $foo) then
# we'll take advantage of that here. Until then, we'll use various hacks
# to tell the compiler when we want a lexical to be a particular type
# or to be a register.
#
sub load_pad {
    my ($namelistav, $valuelistav) = @_;
    @padlist = @_;
    my @namelist = $namelistav->ARRAY;
    my @valuelist = $valuelistav->ARRAY;
    my $ix;
    @pad = ();
    debug "load_pad: $#namelist names, $#valuelist values\n" if $debug_pad;
    # Temporary lexicals don't get named so it's possible for @valuelist
    # to be strictly longer than @namelist. We count $ix up to the end of
    # @valuelist but index into @namelist for the name. Any temporaries which
    # run off the end of @namelist will make $namesv undefined and we treat
    # that the same as having an explicit SPECIAL sv_undef object in @namelist.
    # [XXX If/when @_ becomes a lexical, we must start at 0 here.]
    for ($ix = 1; $ix < @valuelist; $ix++) {
	my $namesv = $namelist[$ix];
	my $type = T_UNKNOWN;
	my $flags = 0;
	my $name = "tmp$ix";
	my $class = class($namesv);
	if (!defined($namesv) || $class eq "SPECIAL") {
	    # temporaries have &PL_sv_undef instead of a PVNV for a name
	    $flags = VALID_SV|TEMPORARY|REGISTER;
	} else {
	    if ($namesv->PV =~ /^\$(.*)_([di])(r?)$/) {
		$name = $1;
		if ($2 eq "i") {
		    $type = T_INT;
		    $flags = VALID_SV|VALID_INT;
		} elsif ($2 eq "d") {
		    $type = T_DOUBLE;
		    $flags = VALID_SV|VALID_DOUBLE;
		}
		$flags |= REGISTER if $3;
	    }
	}
	$pad[$ix] = new B::Stackobj::Padsv ($type, $flags, $ix,
					    "i_$name", "d_$name");

	debug sprintf("PL_curpad[$ix] = %s\n", $pad[$ix]->peek) if $debug_pad;
    }
}

sub declare_pad {
    my $ix;
    for ($ix = 1; $ix <= $#pad; $ix++) {
	my $type = $pad[$ix]->{type};
	declare("IV", $type == T_INT ? 
		sprintf("%s=0",$pad[$ix]->{iv}):$pad[$ix]->{iv}) if $pad[$ix]->save_int;
	declare("double", $type == T_DOUBLE ?
		 sprintf("%s = 0",$pad[$ix]->{nv}):$pad[$ix]->{nv} )if $pad[$ix]->save_double;

    }
}
#
# Debugging stuff
#
sub peek_stack { sprintf "stack = %s\n", join(" ", map($_->minipeek, @stack)) }

#
# OP stuff
#

sub label {
    my $op = shift;
    # XXX Preserve original label name for "real" labels?
    return sprintf("lab_%x", $$op);
}

sub write_label {
    my $op = shift;
    push_runtime(sprintf("  %s:", label($op)));
}

sub loadop {
    my $op = shift;
    my $opsym = $op->save;
    runtime("PL_op = $opsym;") unless $know_op;
    return $opsym;
}

sub doop {
    my $op = shift;
    my $ppname = $op->ppaddr;
    my $sym = loadop($op);
    runtime("DOOP($ppname);");
    $know_op = 1;
    return $sym;
}

sub gimme {
    my $op = shift;
    my $flags = $op->flags;
    return (($flags & OPf_WANT) ? (($flags & OPf_WANT)== OPf_WANT_LIST? G_ARRAY:G_SCALAR) : "dowantarray()");
}

#
# Code generation for PP code
#

sub pp_null {
    my $op = shift;
    return $op->next;
}

sub pp_stub {
    my $op = shift;
    my $gimme = gimme($op);
    if ($gimme != G_ARRAY) {
	my $obj= new B::Stackobj::Const(sv_undef);
    	push(@stack, $obj);
	# XXX Change to push a constant sv_undef Stackobj onto @stack
	#write_back_stack();
	#runtime("if ($gimme != G_ARRAY) XPUSHs(&PL_sv_undef);");
    }
    return $op->next;
}

sub pp_unstack {
    my $op = shift;
    @stack = ();
    runtime("PP_UNSTACK;");
    return $op->next;
}

sub pp_and {
    my $op = shift;
    my $next = $op->next;
    reload_lexicals();
    unshift(@bblock_todo, $next);
    if (@stack >= 1) {
	my $bool = pop_bool();
	write_back_stack();
        save_or_restore_lexical_state($$next);
	runtime(sprintf("if (!$bool) {XPUSHs(&PL_sv_no); goto %s;}", label($next)));
    } else {
        save_or_restore_lexical_state($$next);
	runtime(sprintf("if (!%s) goto %s;", top_bool(), label($next)),
		"*sp--;");
    }
    return $op->other;
}
	    
sub pp_or {
    my $op = shift;
    my $next = $op->next;
    reload_lexicals();
    unshift(@bblock_todo, $next);
    if (@stack >= 1) {
	my $bool = pop_bool @stack;
	write_back_stack();
        save_or_restore_lexical_state($$next);
	runtime(sprintf("if (%s) { XPUSHs(&PL_sv_yes); goto %s; }",
			$bool, label($next)));
    } else {
        save_or_restore_lexical_state($$next);
	runtime(sprintf("if (%s) goto %s;", top_bool(), label($next)),
		"*sp--;");
    }
    return $op->other;
}
	    
sub pp_cond_expr {
    my $op = shift;
    my $false = $op->next;
    unshift(@bblock_todo, $false);
    reload_lexicals();
    my $bool = pop_bool();
    write_back_stack();
    save_or_restore_lexical_state($$false);
    runtime(sprintf("if (!$bool) goto %s;", label($false)));
    return $op->other;
}

sub pp_padsv {
    my $op = shift;
    my $ix = $op->targ;
    push(@stack, $pad[$ix]);
    if ($op->flags & OPf_MOD) {
	my $private = $op->private;
	if ($private & OPpLVAL_INTRO) {
	    runtime("SAVECLEARSV(PL_curpad[$ix]);");
	} elsif ($private & OPpDEREF) {
	    runtime(sprintf("vivify_ref(PL_curpad[%d], %d);",
			    $ix, $private & OPpDEREF));
	    $pad[$ix]->invalidate;
	}
    }
    return $op->next;
}

sub pp_const {
    my $op = shift;
    my $sv = $op->sv;
    my $obj;
    # constant could be in the pad (under useithreads)
    if ($$sv) {
	$obj = $constobj{$$sv};
	if (!defined($obj)) {
	    $obj = $constobj{$$sv} = new B::Stackobj::Const ($sv);
	}
    }
    else {
	$obj = $pad[$op->targ];
    }
    push(@stack, $obj);
    return $op->next;
}

sub pp_nextstate {
    my $op = shift;
    $curcop->load($op);
    @stack = ();
    debug(sprintf("%s:%d\n", $op->file, $op->line)) if $debug_lineno;
    runtime("TAINT_NOT;") unless $omit_taint;
    runtime("sp = PL_stack_base + cxstack[cxstack_ix].blk_oldsp;");
    if ($freetmps_each_bblock || $freetmps_each_loop) {
	$need_freetmps = 1;
    } else {
	runtime("FREETMPS;");
    }
    return $op->next;
}

sub pp_dbstate {
    my $op = shift;
    $curcop->invalidate; # XXX?
    return default_pp($op);
}

#default_pp will handle this:
#sub pp_bless { $curcop->write_back; default_pp(@_) }
#sub pp_repeat { $curcop->write_back; default_pp(@_) }
# The following subs need $curcop->write_back if we decide to support arybase:
# pp_pos, pp_substr, pp_index, pp_rindex, pp_aslice, pp_lslice, pp_splice
#sub pp_caller { $curcop->write_back; default_pp(@_) }
#sub pp_reset { $curcop->write_back; default_pp(@_) }

sub pp_rv2gv{
    my $op =shift;
    $curcop->write_back;
    write_back_lexicals() unless $skip_lexicals{$ppname};
    write_back_stack() unless $skip_stack{$ppname};
    my $sym=doop($op);
    if ($op->private & OPpDEREF) {
        $init->add(sprintf("((UNOP *)$sym)->op_first = $sym;"));	
        $init->add(sprintf("((UNOP *)$sym)->op_type = %d;", 
		$op->first->type));	
    }
    return $op->next;
}
sub pp_sort {
    my $op = shift;
    my $ppname = $op->ppaddr;
    if ( $op->flags & OPf_SPECIAL && $op->flags  & OPf_STACKED){   
        #this indicates the sort BLOCK Array case
        #ugly surgery required.
        my $root=$op->first->sibling->first;
        my $start=$root->first;
	$op->first->save;
	$op->first->sibling->save;
	$root->save;
	my $sym=$start->save;
        my $fakeop=cc_queue("pp_sort".$$op,$root,$start);
	$init->add(sprintf("(%s)->op_next=%s;",$sym,$fakeop));
    }
    $curcop->write_back;
    write_back_lexicals();
    write_back_stack();
    doop($op);
    return $op->next;
}

sub pp_gv {
    my $op = shift;
    my $gvsym;
    if ($Config{useithreads}) {
	$gvsym = $pad[$op->padix]->as_sv;
    }
    else {
	$gvsym = $op->gv->save;
    }
    write_back_stack();
    runtime("XPUSHs((SV*)$gvsym);");
    return $op->next;
}

sub pp_gvsv {
    my $op = shift;
    my $gvsym;
    if ($Config{useithreads}) {
	$gvsym = $pad[$op->padix]->as_sv;
    }
    else {
	$gvsym = $op->gv->save;
    }
    write_back_stack();
    if ($op->private & OPpLVAL_INTRO) {
	runtime("XPUSHs(save_scalar($gvsym));");
    } else {
	runtime("XPUSHs(GvSV($gvsym));");
    }
    return $op->next;
}

sub pp_aelemfast {
    my $op = shift;
    my $gvsym;
    if ($Config{useithreads}) {
	$gvsym = $pad[$op->padix]->as_sv;
    }
    else {
	$gvsym = $op->gv->save;
    }
    my $ix = $op->private;
    my $flag = $op->flags & OPf_MOD;
    write_back_stack();
    runtime("svp = av_fetch(GvAV($gvsym), $ix, $flag);",
	    "PUSHs(svp ? *svp : &PL_sv_undef);");
    return $op->next;
}

sub int_binop {
    my ($op, $operator) = @_;
    if ($op->flags & OPf_STACKED) {
	my $right = pop_int();
	if (@stack >= 1) {
	    my $left = top_int();
	    $stack[-1]->set_int(&$operator($left, $right));
	} else {
	    runtime(sprintf("sv_setiv(TOPs, %s);",&$operator("TOPi", $right)));
	}
    } else {
	my $targ = $pad[$op->targ];
	my $right = new B::Pseudoreg ("IV", "riv");
	my $left = new B::Pseudoreg ("IV", "liv");
	runtime(sprintf("$$right = %s; $$left = %s;", pop_int(), pop_int));
	$targ->set_int(&$operator($$left, $$right));
	push(@stack, $targ);
    }
    return $op->next;
}

sub INTS_CLOSED () { 0x1 }
sub INT_RESULT () { 0x2 }
sub NUMERIC_RESULT () { 0x4 }

sub numeric_binop {
    my ($op, $operator, $flags) = @_;
    my $force_int = 0;
    $force_int ||= ($flags & INT_RESULT);
    $force_int ||= ($flags & INTS_CLOSED && @stack >= 2
		    && valid_int($stack[-2]) && valid_int($stack[-1]));
    if ($op->flags & OPf_STACKED) {
	my $right = pop_numeric();
	if (@stack >= 1) {
	    my $left = top_numeric();
	    if ($force_int) {
		$stack[-1]->set_int(&$operator($left, $right));
	    } else {
		$stack[-1]->set_numeric(&$operator($left, $right));
	    }
	} else {
	    if ($force_int) {
	        my $rightruntime = new B::Pseudoreg ("IV", "riv");
	    	runtime(sprintf("$$rightruntime = %s;",$right));
		runtime(sprintf("sv_setiv(TOPs, %s);",
				&$operator("TOPi", $$rightruntime)));
	    } else {
	    	my $rightruntime = new B::Pseudoreg ("double", "rnv");
	    	runtime(sprintf("$$rightruntime = %s;",$right));
		runtime(sprintf("sv_setnv(TOPs, %s);",
				&$operator("TOPn",$$rightruntime)));
	    }
	}
    } else {
	my $targ = $pad[$op->targ];
	$force_int ||= ($targ->{type} == T_INT);
	if ($force_int) {
	    my $right = new B::Pseudoreg ("IV", "riv");
	    my $left = new B::Pseudoreg ("IV", "liv");
	    runtime(sprintf("$$right = %s; $$left = %s;",
			    pop_numeric(), pop_numeric));
	    $targ->set_int(&$operator($$left, $$right));
	} else {
	    my $right = new B::Pseudoreg ("double", "rnv");
	    my $left = new B::Pseudoreg ("double", "lnv");
	    runtime(sprintf("$$right = %s; $$left = %s;",
			    pop_numeric(), pop_numeric));
	    $targ->set_numeric(&$operator($$left, $$right));
	}
	push(@stack, $targ);
    }
    return $op->next;
}

sub pp_ncmp {
    my ($op) = @_;
    if ($op->flags & OPf_STACKED) {
	my $right = pop_numeric();
	if (@stack >= 1) {
	    my $left = top_numeric();
	    runtime sprintf("if (%s > %s){",$left,$right);
		$stack[-1]->set_int(1);
	    $stack[-1]->write_back();
	    runtime sprintf("}else if (%s < %s ) {",$left,$right);
		$stack[-1]->set_int(-1);
	    $stack[-1]->write_back();
	    runtime sprintf("}else if (%s == %s) {",$left,$right);
		$stack[-1]->set_int(0);
	    $stack[-1]->write_back();
	    runtime sprintf("}else {"); 
		$stack[-1]->set_sv("&PL_sv_undef");
	    runtime "}";
	} else {
	    my $rightruntime = new B::Pseudoreg ("double", "rnv");
	    runtime(sprintf("$$rightruntime = %s;",$right));
	    runtime sprintf(qq/if ("TOPn" > %s){/,$rightruntime);
	    runtime sprintf("sv_setiv(TOPs,1);");
	    runtime sprintf(qq/}else if ( "TOPn" < %s ) {/,$$rightruntime);
	    runtime sprintf("sv_setiv(TOPs,-1);");
	    runtime sprintf(qq/} else if ("TOPn" == %s) {/,$$rightruntime);
	    runtime sprintf("sv_setiv(TOPs,0);");
	    runtime sprintf(qq/}else {/); 
	    runtime sprintf("sv_setiv(TOPs,&PL_sv_undef;");
	    runtime "}";
	}
    } else {
       	my $targ = $pad[$op->targ];
	 my $right = new B::Pseudoreg ("double", "rnv");
	 my $left = new B::Pseudoreg ("double", "lnv");
	 runtime(sprintf("$$right = %s; $$left = %s;",
			    pop_numeric(), pop_numeric));
	runtime sprintf("if (%s > %s){",$$left,$$right);
		$targ->set_int(1);
		$targ->write_back();
	runtime sprintf("}else if (%s < %s ) {",$$left,$$right);
		$targ->set_int(-1);
		$targ->write_back();
	runtime sprintf("}else if (%s == %s) {",$$left,$$right);
		$targ->set_int(0);
		$targ->write_back();
	runtime sprintf("}else {"); 
		$targ->set_sv("&PL_sv_undef");
	runtime "}";
	push(@stack, $targ);
    }
    return $op->next;
}

sub sv_binop {
    my ($op, $operator, $flags) = @_;
    if ($op->flags & OPf_STACKED) {
	my $right = pop_sv();
	if (@stack >= 1) {
	    my $left = top_sv();
	    if ($flags & INT_RESULT) {
		$stack[-1]->set_int(&$operator($left, $right));
	    } elsif ($flags & NUMERIC_RESULT) {
		$stack[-1]->set_numeric(&$operator($left, $right));
	    } else {
		# XXX Does this work?
		runtime(sprintf("sv_setsv($left, %s);",
				&$operator($left, $right)));
		$stack[-1]->invalidate;
	    }
	} else {
	    my $f;
	    if ($flags & INT_RESULT) {
		$f = "sv_setiv";
	    } elsif ($flags & NUMERIC_RESULT) {
		$f = "sv_setnv";
	    } else {
		$f = "sv_setsv";
	    }
	    runtime(sprintf("%s(TOPs, %s);", $f, &$operator("TOPs", $right)));
	}
    } else {
	my $targ = $pad[$op->targ];
	runtime(sprintf("right = %s; left = %s;", pop_sv(), pop_sv));
	if ($flags & INT_RESULT) {
	    $targ->set_int(&$operator("left", "right"));
	} elsif ($flags & NUMERIC_RESULT) {
	    $targ->set_numeric(&$operator("left", "right"));
	} else {
	    # XXX Does this work?
	    runtime(sprintf("sv_setsv(%s, %s);",
			    $targ->as_sv, &$operator("left", "right")));
	    $targ->invalidate;
	}
	push(@stack, $targ);
    }
    return $op->next;
}
    
sub bool_int_binop {
    my ($op, $operator) = @_;
    my $right = new B::Pseudoreg ("IV", "riv");
    my $left = new B::Pseudoreg ("IV", "liv");
    runtime(sprintf("$$right = %s; $$left = %s;", pop_int(), pop_int()));
    my $bool = new B::Stackobj::Bool (new B::Pseudoreg ("int", "b"));
    $bool->set_int(&$operator($$left, $$right));
    push(@stack, $bool);
    return $op->next;
}

sub bool_numeric_binop {
    my ($op, $operator) = @_;
    my $right = new B::Pseudoreg ("double", "rnv");
    my $left = new B::Pseudoreg ("double", "lnv");
    runtime(sprintf("$$right = %s; $$left = %s;",
		    pop_numeric(), pop_numeric()));
    my $bool = new B::Stackobj::Bool (new B::Pseudoreg ("int", "b"));
    $bool->set_numeric(&$operator($$left, $$right));
    push(@stack, $bool);
    return $op->next;
}

sub bool_sv_binop {
    my ($op, $operator) = @_;
    runtime(sprintf("right = %s; left = %s;", pop_sv(), pop_sv()));
    my $bool = new B::Stackobj::Bool (new B::Pseudoreg ("int", "b"));
    $bool->set_numeric(&$operator("left", "right"));
    push(@stack, $bool);
    return $op->next;
}

sub infix_op {
    my $opname = shift;
    return sub { "$_[0] $opname $_[1]" }
}

sub prefix_op {
    my $opname = shift;
    return sub { sprintf("%s(%s)", $opname, join(", ", @_)) }
}

BEGIN {
    my $plus_op = infix_op("+");
    my $minus_op = infix_op("-");
    my $multiply_op = infix_op("*");
    my $divide_op = infix_op("/");
    my $modulo_op = infix_op("%");
    my $lshift_op = infix_op("<<");
    my $rshift_op = infix_op(">>");
    my $scmp_op = prefix_op("sv_cmp");
    my $seq_op = prefix_op("sv_eq");
    my $sne_op = prefix_op("!sv_eq");
    my $slt_op = sub { "sv_cmp($_[0], $_[1]) < 0" };
    my $sgt_op = sub { "sv_cmp($_[0], $_[1]) > 0" };
    my $sle_op = sub { "sv_cmp($_[0], $_[1]) <= 0" };
    my $sge_op = sub { "sv_cmp($_[0], $_[1]) >= 0" };
    my $eq_op = infix_op("==");
    my $ne_op = infix_op("!=");
    my $lt_op = infix_op("<");
    my $gt_op = infix_op(">");
    my $le_op = infix_op("<=");
    my $ge_op = infix_op(">=");

    #
    # XXX The standard perl PP code has extra handling for
    # some special case arguments of these operators.
    #
    sub pp_add { numeric_binop($_[0], $plus_op) }
    sub pp_subtract { numeric_binop($_[0], $minus_op) }
    sub pp_multiply { numeric_binop($_[0], $multiply_op) }
    sub pp_divide { numeric_binop($_[0], $divide_op) }
    sub pp_modulo { int_binop($_[0], $modulo_op) } # differs from perl's

    sub pp_left_shift { int_binop($_[0], $lshift_op) }
    sub pp_right_shift { int_binop($_[0], $rshift_op) }
    sub pp_i_add { int_binop($_[0], $plus_op) }
    sub pp_i_subtract { int_binop($_[0], $minus_op) }
    sub pp_i_multiply { int_binop($_[0], $multiply_op) }
    sub pp_i_divide { int_binop($_[0], $divide_op) }
    sub pp_i_modulo { int_binop($_[0], $modulo_op) }

    sub pp_eq { bool_numeric_binop($_[0], $eq_op) }
    sub pp_ne { bool_numeric_binop($_[0], $ne_op) }
    sub pp_lt { bool_numeric_binop($_[0], $lt_op) }
    sub pp_gt { bool_numeric_binop($_[0], $gt_op) }
    sub pp_le { bool_numeric_binop($_[0], $le_op) }
    sub pp_ge { bool_numeric_binop($_[0], $ge_op) }

    sub pp_i_eq { bool_int_binop($_[0], $eq_op) }
    sub pp_i_ne { bool_int_binop($_[0], $ne_op) }
    sub pp_i_lt { bool_int_binop($_[0], $lt_op) }
    sub pp_i_gt { bool_int_binop($_[0], $gt_op) }
    sub pp_i_le { bool_int_binop($_[0], $le_op) }
    sub pp_i_ge { bool_int_binop($_[0], $ge_op) }

    sub pp_scmp { sv_binop($_[0], $scmp_op, INT_RESULT) }
    sub pp_slt { bool_sv_binop($_[0], $slt_op) }
    sub pp_sgt { bool_sv_binop($_[0], $sgt_op) }
    sub pp_sle { bool_sv_binop($_[0], $sle_op) }
    sub pp_sge { bool_sv_binop($_[0], $sge_op) }
    sub pp_seq { bool_sv_binop($_[0], $seq_op) }
    sub pp_sne { bool_sv_binop($_[0], $sne_op) }
}


sub pp_sassign {
    my $op = shift;
    my $backwards = $op->private & OPpASSIGN_BACKWARDS;
    my ($dst, $src);
    if (@stack >= 2) {
	$dst = pop @stack;
	$src = pop @stack;
	($src, $dst) = ($dst, $src) if $backwards;
	my $type = $src->{type};
	if ($type == T_INT) {
	    $dst->set_int($src->as_int,$src->{flags} & VALID_UNSIGNED);
	} elsif ($type == T_DOUBLE) {
	    $dst->set_numeric($src->as_numeric);
	} else {
	    $dst->set_sv($src->as_sv);
	}
	push(@stack, $dst);
    } elsif (@stack == 1) {
	if ($backwards) {
	    my $src = pop @stack;
	    my $type = $src->{type};
	    runtime("if (PL_tainting && PL_tainted) TAINT_NOT;");
	    if ($type == T_INT) {
                if ($src->{flags} & VALID_UNSIGNED){ 
                     runtime sprintf("sv_setuv(TOPs, %s);", $src->as_int);
                }else{
                    runtime sprintf("sv_setiv(TOPs, %s);", $src->as_int);
                }
	    } elsif ($type == T_DOUBLE) {
		runtime sprintf("sv_setnv(TOPs, %s);", $src->as_double);
	    } else {
		runtime sprintf("sv_setsv(TOPs, %s);", $src->as_sv);
	    }
	    runtime("SvSETMAGIC(TOPs);");
	} else {
	    my $dst = $stack[-1];
	    my $type = $dst->{type};
	    runtime("sv = POPs;");
	    runtime("MAYBE_TAINT_SASSIGN_SRC(sv);");
	    if ($type == T_INT) {
		$dst->set_int("SvIV(sv)");
	    } elsif ($type == T_DOUBLE) {
		$dst->set_double("SvNV(sv)");
	    } else {
		runtime("SvSetMagicSV($dst->{sv}, sv);");
		$dst->invalidate;
	    }
	}
    } else {
	if ($backwards) {
	    runtime("src = POPs; dst = TOPs;");
	} else {
	    runtime("dst = POPs; src = TOPs;");
	}
	runtime("MAYBE_TAINT_SASSIGN_SRC(src);",
		"SvSetSV(dst, src);",
		"SvSETMAGIC(dst);",
		"SETs(dst);");
    }
    return $op->next;
}

sub pp_preinc {
    my $op = shift;
    if (@stack >= 1) {
	my $obj = $stack[-1];
	my $type = $obj->{type};
	if ($type == T_INT || $type == T_DOUBLE) {
	    $obj->set_int($obj->as_int . " + 1");
	} else {
	    runtime sprintf("PP_PREINC(%s);", $obj->as_sv);
	    $obj->invalidate();
	}
    } else {
	runtime sprintf("PP_PREINC(TOPs);");
    }
    return $op->next;
}


sub pp_pushmark {
    my $op = shift;
    write_back_stack();
    runtime("PUSHMARK(sp);");
    return $op->next;
}

sub pp_list {
    my $op = shift;
    write_back_stack();
    my $gimme = gimme($op);
    if ($gimme == G_ARRAY) { # sic
	runtime("POPMARK;"); # need this even though not a "full" pp_list
    } else {
	runtime("PP_LIST($gimme);");
    }
    return $op->next;
}

sub pp_entersub {
    my $op = shift;
    $curcop->write_back;
    write_back_lexicals(REGISTER|TEMPORARY);
    write_back_stack();
    my $sym = doop($op);
    runtime("while (PL_op != ($sym)->op_next && PL_op != (OP*)0 ){");
    runtime("PL_op = (*PL_op->op_ppaddr)(aTHX);");
    runtime("SPAGAIN;}");
    $know_op = 0;
    invalidate_lexicals(REGISTER|TEMPORARY);
    return $op->next;
}
sub pp_formline {
    my $op = shift;
    my $ppname = $op->ppaddr;
    write_back_lexicals() unless $skip_lexicals{$ppname};
    write_back_stack() unless $skip_stack{$ppname};
    my $sym=doop($op);
    # See comment in pp_grepwhile to see why!
    $init->add("((LISTOP*)$sym)->op_first = $sym;");    
    runtime("if (PL_op == ((LISTOP*)($sym))->op_first){");
    save_or_restore_lexical_state(${$op->first});
    runtime( sprintf("goto %s;",label($op->first)));
    runtime("}");
    return $op->next;
}

sub pp_goto{

    my $op = shift;
    my $ppname = $op->ppaddr;
    write_back_lexicals() unless $skip_lexicals{$ppname};
    write_back_stack() unless $skip_stack{$ppname};
    my $sym=doop($op);
    runtime("if (PL_op != ($sym)->op_next && PL_op != (OP*)0){return PL_op;}");
    invalidate_lexicals() unless $skip_invalidate{$ppname};
    return $op->next;
}
sub pp_enterwrite {
    my $op = shift;
    pp_entersub($op);
}
sub pp_leavesub{
    my $op = shift;
    write_back_lexicals() unless $skip_lexicals{$ppname};
    write_back_stack() unless $skip_stack{$ppname};
    runtime("if (PL_curstackinfo->si_type == PERLSI_SORT){");   
    runtime("\tPUTBACK;return 0;");
    runtime("}");
    doop($op);
    return $op->next;
}
sub pp_leavewrite {
    my $op = shift;
    write_back_lexicals(REGISTER|TEMPORARY);
    write_back_stack();
    my $sym = doop($op);
    # XXX Is this the right way to distinguish between it returning
    # CvSTART(cv) (via doform) and pop_return()?
    #runtime("if (PL_op) PL_op = (*PL_op->op_ppaddr)(aTHX);");
    runtime("SPAGAIN;");
    $know_op = 0;
    invalidate_lexicals(REGISTER|TEMPORARY);
    return $op->next;
}

sub doeval {
    my $op = shift;
    $curcop->write_back;
    write_back_lexicals(REGISTER|TEMPORARY);
    write_back_stack();
    my $sym = loadop($op);
    my $ppaddr = $op->ppaddr;
    #runtime(qq/printf("$ppaddr type eval\n");/);
    runtime("PP_EVAL($ppaddr, ($sym)->op_next);");
    $know_op = 1;
    invalidate_lexicals(REGISTER|TEMPORARY);
    return $op->next;
}

sub pp_entereval { doeval(@_) }
sub pp_dofile { doeval(@_) }

#pp_require is protected by pp_entertry, so no protection for it.
sub pp_require {
    my $op = shift;
    $curcop->write_back;
    write_back_lexicals(REGISTER|TEMPORARY);
    write_back_stack();
    my $sym = doop($op);
    runtime("while (PL_op != ($sym)->op_next && PL_op != (OP*)0 ){");
    runtime("PL_op = (*PL_op->op_ppaddr)(ARGS);");
    runtime("SPAGAIN;}");
    $know_op = 1;
    invalidate_lexicals(REGISTER|TEMPORARY);
    return $op->next;
}


sub pp_entertry {
    my $op = shift;
    $curcop->write_back;
    write_back_lexicals(REGISTER|TEMPORARY);
    write_back_stack();
    my $sym = doop($op);
    my $jmpbuf = sprintf("jmpbuf%d", $jmpbuf_ix++);
    declare("JMPENV", $jmpbuf);
    runtime(sprintf("PP_ENTERTRY(%s,%s);", $jmpbuf, label($op->other->next)));
    invalidate_lexicals(REGISTER|TEMPORARY);
    return $op->next;
}

sub pp_leavetry{
	my $op=shift;
	default_pp($op);
	runtime("PP_LEAVETRY;");
    	return $op->next;
}

sub pp_grepstart {
    my $op = shift;
    if ($need_freetmps && $freetmps_each_loop) {
	runtime("FREETMPS;"); # otherwise the grepwhile loop messes things up
	$need_freetmps = 0;
    }
    write_back_stack();
    my $sym= doop($op);
    my $next=$op->next;
    $next->save;
    my $nexttonext=$next->next;
    $nexttonext->save;
    save_or_restore_lexical_state($$nexttonext);
    runtime(sprintf("if (PL_op == (($sym)->op_next)->op_next) goto %s;",
		    label($nexttonext)));
    return $op->next->other;
}

sub pp_mapstart {
    my $op = shift;
    if ($need_freetmps && $freetmps_each_loop) {
	runtime("FREETMPS;"); # otherwise the mapwhile loop messes things up
	$need_freetmps = 0;
    }
    write_back_stack();
    # pp_mapstart can return either op_next->op_next or op_next->op_other and
    # we need to be able to distinguish the two at runtime. 
    my $sym= doop($op);
    my $next=$op->next;
    $next->save;
    my $nexttonext=$next->next;
    $nexttonext->save;
    save_or_restore_lexical_state($$nexttonext);
    runtime(sprintf("if (PL_op == (($sym)->op_next)->op_next) goto %s;",
		    label($nexttonext)));
    return $op->next->other;
}

sub pp_grepwhile {
    my $op = shift;
    my $next = $op->next;
    unshift(@bblock_todo, $next);
    write_back_lexicals();
    write_back_stack();
    my $sym = doop($op);
    # pp_grepwhile can return either op_next or op_other and we need to
    # be able to distinguish the two at runtime. Since it's possible for
    # both ops to be "inlined", the fields could both be zero. To get
    # around that, we hack op_next to be our own op (purely because we
    # know it's a non-NULL pointer and can't be the same as op_other).
    $init->add("((LOGOP*)$sym)->op_next = $sym;");
    save_or_restore_lexical_state($$next);
    runtime(sprintf("if (PL_op == ($sym)->op_next) goto %s;", label($next)));
    $know_op = 0;
    return $op->other;
}

sub pp_mapwhile {
    pp_grepwhile(@_);
}

sub pp_return {
    my $op = shift;
    write_back_lexicals(REGISTER|TEMPORARY);
    write_back_stack();
    doop($op);
    runtime("PUTBACK;", "return PL_op;");
    $know_op = 0;
    return $op->next;
}

sub nyi {
    my $op = shift;
    warn sprintf("%s not yet implemented properly\n", $op->ppaddr);
    return default_pp($op);
}

sub pp_range {
    my $op = shift;
    my $flags = $op->flags;
    if (!($flags & OPf_WANT)) {
	error("context of range unknown at compile-time");
    }
    write_back_lexicals();
    write_back_stack();
    unless (($flags & OPf_WANT)== OPf_WANT_LIST) {
	# We need to save our UNOP structure since pp_flop uses
	# it to find and adjust out targ. We don't need it ourselves.
	$op->save;
        save_or_restore_lexical_state(${$op->other});
	runtime sprintf("if (SvTRUE(PL_curpad[%d])) goto %s;",
			$op->targ, label($op->other));
	unshift(@bblock_todo, $op->other);
    }
    return $op->next;
}

sub pp_flip {
    my $op = shift;
    my $flags = $op->flags;
    if (!($flags & OPf_WANT)) {
	error("context of flip unknown at compile-time");
    }
    if (($flags & OPf_WANT)==OPf_WANT_LIST) {
	return $op->first->other;
    }
    write_back_lexicals();
    write_back_stack();
    # We need to save our UNOP structure since pp_flop uses
    # it to find and adjust out targ. We don't need it ourselves.
    $op->save;
    my $ix = $op->targ;
    my $rangeix = $op->first->targ;
    runtime(($op->private & OPpFLIP_LINENUM) ?
	    "if (PL_last_in_gv && SvIV(TOPs) == IoLINES(GvIOp(PL_last_in_gv))) {"
	  : "if (SvTRUE(TOPs)) {");
    runtime("\tsv_setiv(PL_curpad[$rangeix], 1);");
    if ($op->flags & OPf_SPECIAL) {
	runtime("sv_setiv(PL_curpad[$ix], 1);");
    } else {
    	save_or_restore_lexical_state(${$op->first->other});
	runtime("\tsv_setiv(PL_curpad[$ix], 0);",
		"\tsp--;",
		sprintf("\tgoto %s;", label($op->first->other)));
    }
    runtime("}",
	  qq{sv_setpv(PL_curpad[$ix], "");},
	    "SETs(PL_curpad[$ix]);");
    $know_op = 0;
    return $op->next;
}

sub pp_flop {
    my $op = shift;
    default_pp($op);
    $know_op = 0;
    return $op->next;
}

sub enterloop {
    my $op = shift;
    my $nextop = $op->nextop;
    my $lastop = $op->lastop;
    my $redoop = $op->redoop;
    $curcop->write_back;
    debug "enterloop: pushing on cxstack" if $debug_cxstack;
    push(@cxstack, {
	type => CXt_LOOP,
	op => $op,
	"label" => $curcop->[0]->label,
	nextop => $nextop,
	lastop => $lastop,
	redoop => $redoop
    });
    $nextop->save;
    $lastop->save;
    $redoop->save;
    return default_pp($op);
}

sub pp_enterloop { enterloop(@_) }
sub pp_enteriter { enterloop(@_) }

sub pp_leaveloop {
    my $op = shift;
    if (!@cxstack) {
	die "panic: leaveloop";
    }
    debug "leaveloop: popping from cxstack" if $debug_cxstack;
    pop(@cxstack);
    return default_pp($op);
}

sub pp_next {
    my $op = shift;
    my $cxix;
    if ($op->flags & OPf_SPECIAL) {
	$cxix = dopoptoloop();
	if ($cxix < 0) {
	    error('"next" used outside loop');
	    return $op->next; # ignore the op
	}
    } else {
	$cxix = dopoptolabel($op->pv);
	if ($cxix < 0) {
	    error('Label not found at compile time for "next %s"', $op->pv);
	    return $op->next; # ignore the op
	}
    }
    default_pp($op);
    my $nextop = $cxstack[$cxix]->{nextop};
    push(@bblock_todo, $nextop);
    save_or_restore_lexical_state($$nextop);
    runtime(sprintf("goto %s;", label($nextop)));
    return $op->next;
}

sub pp_redo {
    my $op = shift;
    my $cxix;
    if ($op->flags & OPf_SPECIAL) {
	$cxix = dopoptoloop();
	if ($cxix < 0) {
	    error('"redo" used outside loop');
	    return $op->next; # ignore the op
	}
    } else {
	$cxix = dopoptolabel($op->pv);
	if ($cxix < 0) {
	    error('Label not found at compile time for "redo %s"', $op->pv);
	    return $op->next; # ignore the op
	}
    }
    default_pp($op);
    my $redoop = $cxstack[$cxix]->{redoop};
    push(@bblock_todo, $redoop);
    save_or_restore_lexical_state($$redoop);
    runtime(sprintf("goto %s;", label($redoop)));
    return $op->next;
}

sub pp_last {
    my $op = shift;
    my $cxix;
    if ($op->flags & OPf_SPECIAL) {
	$cxix = dopoptoloop();
	if ($cxix < 0) {
	    error('"last" used outside loop');
	    return $op->next; # ignore the op
	}
    } else {
	$cxix = dopoptolabel($op->pv);
	if ($cxix < 0) {
	    error('Label not found at compile time for "last %s"', $op->pv);
	    return $op->next; # ignore the op
	}
	# XXX Add support for "last" to leave non-loop blocks
	if ($cxstack[$cxix]->{type} != CXt_LOOP) {
	    error('Use of "last" for non-loop blocks is not yet implemented');
	    return $op->next; # ignore the op
	}
    }
    default_pp($op);
    my $lastop = $cxstack[$cxix]->{lastop}->next;
    push(@bblock_todo, $lastop);
    save_or_restore_lexical_state($$lastop);
    runtime(sprintf("goto %s;", label($lastop)));
    return $op->next;
}

sub pp_subst {
    my $op = shift;
    write_back_lexicals();
    write_back_stack();
    my $sym = doop($op);
    my $replroot = $op->pmreplroot;
    if ($$replroot) {
        save_or_restore_lexical_state($$replroot);
	runtime sprintf("if (PL_op == ((PMOP*)(%s))->op_pmreplroot) goto %s;",
			$sym, label($replroot));
	$op->pmreplstart->save;
	push(@bblock_todo, $replroot);
    }
    invalidate_lexicals();
    return $op->next;
}

sub pp_substcont {
    my $op = shift;
    write_back_lexicals();
    write_back_stack();
    doop($op);
    my $pmop = $op->other;
    # warn sprintf("substcont: op = %s, pmop = %s\n",
    # 		 peekop($op), peekop($pmop));#debug
#   my $pmopsym = objsym($pmop);
    my $pmopsym = $pmop->save; # XXX can this recurse?
#   warn "pmopsym = $pmopsym\n";#debug
    save_or_restore_lexical_state(${$pmop->pmreplstart});
    runtime sprintf("if (PL_op == ((PMOP*)(%s))->op_pmreplstart) goto %s;",
		    $pmopsym, label($pmop->pmreplstart));
    invalidate_lexicals();
    return $pmop->next;
}

sub default_pp {
    my $op = shift;
    my $ppname = "pp_" . $op->name;
    if ($curcop and $need_curcop{$ppname}){
	$curcop->write_back;
    }
    write_back_lexicals() unless $skip_lexicals{$ppname};
    write_back_stack() unless $skip_stack{$ppname};
    doop($op);
    # XXX If the only way that ops can write to a TEMPORARY lexical is
    # when it's named in $op->targ then we could call
    # invalidate_lexicals(TEMPORARY) and avoid having to write back all
    # the temporaries. For now, we'll play it safe and write back the lot.
    invalidate_lexicals() unless $skip_invalidate{$ppname};
    return $op->next;
}

sub compile_op {
    my $op = shift;
    my $ppname = "pp_" . $op->name;
    if (exists $ignore_op{$ppname}) {
	return $op->next;
    }
    debug peek_stack() if $debug_stack;
    if ($debug_op) {
	debug sprintf("%s [%s]\n",
		     peekop($op),
		     $op->flags & OPf_STACKED ? "OPf_STACKED" : $op->targ);
    }
    no strict 'refs';
    if (defined(&$ppname)) {
	$know_op = 0;
	return &$ppname($op);
    } else {
	return default_pp($op);
    }
}

sub compile_bblock {
    my $op = shift;
    #warn "compile_bblock: ", peekop($op), "\n"; # debug
    save_or_restore_lexical_state($$op);
    write_label($op);
    $know_op = 0;
    do {
	$op = compile_op($op);
    } while (defined($op) && $$op && !exists($leaders->{$$op}));
    write_back_stack(); # boo hoo: big loss
    reload_lexicals();
    return $op;
}

sub cc {
    my ($name, $root, $start, @padlist) = @_;
    my $op;
    if($done{$$start}){ 
    	#warn "repeat=>".ref($start)."$name,\n";#debug
	$decl->add(sprintf("#define $name  %s",$done{$$start}));
	return;
    }
    init_pp($name);
    load_pad(@padlist);
    %lexstate=();
    B::Pseudoreg->new_scope;
    @cxstack = ();
    if ($debug_timings) {
	warn sprintf("Basic block analysis at %s\n", timing_info);
    }
    $leaders = find_leaders($root, $start);
    my @leaders= keys %$leaders; 
    if ($#leaders > -1) { 
    	@bblock_todo = ($start, values %$leaders) ;
    } else{
	runtime("return PL_op?PL_op->op_next:0;");
    }
    if ($debug_timings) {
	warn sprintf("Compilation at %s\n", timing_info);
    }
    while (@bblock_todo) {
	$op = shift @bblock_todo;
	#warn sprintf("Considering basic block %s\n", peekop($op)); # debug
	next if !defined($op) || !$$op || $done{$$op};
	#warn "...compiling it\n"; # debug
	do {
	    $done{$$op} = $name;
	    $op = compile_bblock($op);
	    if ($need_freetmps && $freetmps_each_bblock) {
		runtime("FREETMPS;");
		$need_freetmps = 0;
	    }
	} while defined($op) && $$op && !$done{$$op};
	if ($need_freetmps && $freetmps_each_loop) {
	    runtime("FREETMPS;");
	    $need_freetmps = 0;
	}
	if (!$$op) {
	    runtime("PUTBACK;","return PL_op;");
	} elsif ($done{$$op}) {
    	    save_or_restore_lexical_state($$op);
	    runtime(sprintf("goto %s;", label($op)));
	}
    }
    if ($debug_timings) {
	warn sprintf("Saving runtime at %s\n", timing_info);
    }
    declare_pad(@padlist) ;
    save_runtime();
}

sub cc_recurse {
    my $ccinfo;
    my $start;
    $start = cc_queue(@_) if @_;
    while ($ccinfo = shift @cc_todo) {
	cc(@$ccinfo);
    }
    return $start;
}    

sub cc_obj {
    my ($name, $cvref) = @_;
    my $cv = svref_2object($cvref);
    my @padlist = $cv->PADLIST->ARRAY;
    my $curpad_sym = $padlist[1]->save;
    cc_recurse($name, $cv->ROOT, $cv->START, @padlist);
}

sub cc_main {
    my @comppadlist = comppadlist->ARRAY;
    my $curpad_nam  = $comppadlist[0]->save;
    my $curpad_sym  = $comppadlist[1]->save;
    my $init_av     = init_av->save; 
    my $start = cc_recurse("pp_main", main_root, main_start, @comppadlist);
    # Do save_unused_subs before saving inc_hv
    save_unused_subs();
    cc_recurse();

    my $inc_hv      = svref_2object(\%INC)->save;
    my $inc_av      = svref_2object(\@INC)->save;
    my $amagic_generate= amagic_generation;
    return if $errors;
    if (!defined($module)) {
	$init->add(sprintf("PL_main_root = s\\_%x;", ${main_root()}),
		   "PL_main_start = $start;",
		   "PL_curpad = AvARRAY($curpad_sym);",
		   "PL_initav = (AV *) $init_av;",
		   "GvHV(PL_incgv) = $inc_hv;",
		   "GvAV(PL_incgv) = $inc_av;",
		   "av_store(CvPADLIST(PL_main_cv),0,SvREFCNT_inc($curpad_nam));",
		   "av_store(CvPADLIST(PL_main_cv),1,SvREFCNT_inc($curpad_sym));",
		   "PL_amagic_generation= $amagic_generate;",
		     );
                 
    }
    seek(STDOUT,0,0); #prevent print statements from BEGIN{} into the output
    output_boilerplate();
    print "\n";
    output_all("perl_init");
    output_runtime();
    print "\n";
    output_main();
    if (defined($module)) {
	my $cmodule = $module;
	$cmodule =~ s/::/__/g;
	print <<"EOT";

#include "XSUB.h"
XS(boot_$cmodule)
{
    dXSARGS;
    perl_init();
    ENTER;
    SAVETMPS;
    SAVEVPTR(PL_curpad);
    SAVEVPTR(PL_op);
    PL_curpad = AvARRAY($curpad_sym);
    PL_op = $start;
    pp_main(aTHX);
    FREETMPS;
    LEAVE;
    ST(0) = &PL_sv_yes;
    XSRETURN(1);
}
EOT
    }
    if ($debug_timings) {
	warn sprintf("Done at %s\n", timing_info);
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
	} elsif ($opt eq "o") {
	    $arg ||= shift @options;
	    open(STDOUT, ">$arg") or return "open '>$arg': $!\n";
	} elsif ($opt eq "n") {
	    $arg ||= shift @options;
	    $module_name = $arg;
	} elsif ($opt eq "u") {
	    $arg ||= shift @options;
	    mark_unused($arg,undef);
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
		$freetmps_each_loop = 1;
	    }
	    if ($arg >= 1) {
		$freetmps_each_bblock = 1 unless $freetmps_each_loop;
	    }
	} elsif ($opt eq "m") {
	    $arg ||= shift @options;
	    $module = $arg;
	    mark_unused($arg,undef);
	} elsif ($opt eq "p") {
	    $arg ||= shift @options;
	    $patchlevel = $arg;
	} elsif ($opt eq "D") {
            $arg ||= shift @options;
	    foreach $arg (split(//, $arg)) {
		if ($arg eq "o") {
		    B->debug(1);
		} elsif ($arg eq "O") {
		    $debug_op = 1;
		} elsif ($arg eq "s") {
		    $debug_stack = 1;
		} elsif ($arg eq "c") {
		    $debug_cxstack = 1;
		} elsif ($arg eq "p") {
		    $debug_pad = 1;
		} elsif ($arg eq "r") {
		    $debug_runtime = 1;
		} elsif ($arg eq "S") {
		    $debug_shadow = 1;
		} elsif ($arg eq "q") {
		    $debug_queue = 1;
		} elsif ($arg eq "l") {
		    $debug_lineno = 1;
		} elsif ($arg eq "t") {
		    $debug_timings = 1;
		}
	    }
	}
    }
    init_sections();
    $init = B::Section->get("init");
    $decl = B::Section->get("decl");

    if (@options) {
	return sub {
	    my ($objname, $ppname);
	    foreach $objname (@options) {
		$objname = "main::$objname" unless $objname =~ /::/;
		($ppname = $objname) =~ s/^.*?:://;
		eval "cc_obj(qq(pp_sub_$ppname), \\&$objname)";
		die "cc_obj(qq(pp_sub_$ppname, \\&$objname) failed: $@" if $@;
		return if $errors;
	    }
	    output_boilerplate();
	    print "\n";
	    output_all($module_name || "init_module");
	    output_runtime();
	}
    } else {
	return sub { cc_main() };
    }
}

1;

__END__

=head1 NAME

B::CC - Perl compiler's optimized C translation backend

=head1 SYNOPSIS

	perl -MO=CC[,OPTIONS] foo.pl

=head1 DESCRIPTION

This compiler backend takes Perl source and generates C source code
corresponding to the flow of your program. In other words, this
backend is somewhat a "real" compiler in the sense that many people
think about compilers. Note however that, currently, it is a very
poor compiler in that although it generates (mostly, or at least
sometimes) correct code, it performs relatively few optimisations.
This will change as the compiler develops. The result is that
running an executable compiled with this backend may start up more
quickly than running the original Perl program (a feature shared
by the B<C> compiler backend--see F<B::C>) and may also execute
slightly faster. This is by no means a good optimising compiler--yet.

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

=item B<-mModulename>

Instead of generating source for a runnable executable, generate
source for an XSUB module. The boot_Modulename function (which
DynaLoader can look for) does the appropriate initialisation and runs
the main part of the Perl source that is being compiled.


=item B<-D>

Debug options (concatenated or separate flags like C<perl -D>).

=item B<-Dr>

Writes debugging output to STDERR just as it's about to write to the
program's runtime (otherwise writes debugging info as comments in
its C output).

=item B<-DO>

Outputs each OP as it's compiled

=item B<-Ds>

Outputs the contents of the shadow stack at each OP

=item B<-Dp>

Outputs the contents of the shadow pad of lexicals as it's loaded for
each sub or the main program.

=item B<-Dq>

Outputs the name of each fake PP function in the queue as it's about
to process it.

=item B<-Dl>

Output the filename and line number of each original line of Perl
code as it's processed (C<pp_nextstate>).

=item B<-Dt>

Outputs timing information of compilation stages.

=item B<-f>

Force optimisations on or off one at a time.

=item B<-ffreetmps-each-bblock>

Delays FREETMPS from the end of each statement to the end of the each
basic block.

=item B<-ffreetmps-each-loop>

Delays FREETMPS from the end of each statement to the end of the group
of basic blocks forming a loop. At most one of the freetmps-each-*
options can be used.

=item B<-fomit-taint>

Omits generating code for handling perl's tainting mechanism.

=item B<-On>

Optimisation level (n = 0, 1, 2, ...). B<-O> means B<-O1>.
Currently, B<-O1> sets B<-ffreetmps-each-bblock> and B<-O2>
sets B<-ffreetmps-each-loop>.

=back

=head1 EXAMPLES

        perl -MO=CC,-O2,-ofoo.c foo.pl
        perl cc_harness -o foo foo.c

Note that C<cc_harness> lives in the C<B> subdirectory of your perl
library directory. The utility called C<perlcc> may also be used to
help make use of this compiler.

        perl -MO=CC,-mFoo,-oFoo.c Foo.pm
        perl cc_harness -shared -c -o Foo.so Foo.c

=head1 BUGS

Plenty. Current status: experimental.

=head1 DIFFERENCES

These aren't really bugs but they are constructs which are heavily
tied to perl's compile-and-go implementation and with which this
compiler backend cannot cope.

=head2 Loops

Standard perl calculates the target of "next", "last", and "redo"
at run-time. The compiler calculates the targets at compile-time.
For example, the program

    sub skip_on_odd { next NUMBER if $_[0] % 2 }
    NUMBER: for ($i = 0; $i < 5; $i++) {
        skip_on_odd($i);
        print $i;
    }

produces the output

    024

with standard perl but gives a compile-time error with the compiler.

=head2 Context of ".."

The context (scalar or array) of the ".." operator determines whether
it behaves as a range or a flip/flop. Standard perl delays until
runtime the decision of which context it is in but the compiler needs
to know the context at compile-time. For example,

    @a = (4,6,1,0,0,1);
    sub range { (shift @a)..(shift @a) }
    print range();
    while (@a) { print scalar(range()) }

generates the output

    456123E0

with standard Perl but gives a compile-time error with compiled Perl.

=head2 Arithmetic

Compiled Perl programs use native C arithemtic much more frequently
than standard perl. Operations on large numbers or on boundary
cases may produce different behaviour.

=head2 Deprecated features

Features of standard perl such as C<$[> which have been deprecated
in standard perl since Perl5 was released have not been implemented
in the compiler.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
