package B::Concise;
# Copyright (C) 2000, 2001 Stephen McCamant. All rights reserved.
# This program is free software; you can redistribute and/or modify it
# under the same terms as Perl itself.

our $VERSION = "0.51";
use strict;
use B qw(class ppname main_start main_root main_cv cstring svref_2object
	 SVf_IOK SVf_NOK SVf_POK OPf_KIDS);

my %style = 
  ("terse" =>
   ["(?(#label =>\n)?)(*(    )*)#class (#addr) #name (?([#targ])?) "
    . "#svclass~(?((#svaddr))?)~#svval~(?(label \"#coplabel\")?)\n",
    "(*(    )*)goto #class (#addr)\n",
    "#class pp_#name"],
   "concise" =>
   ["#hyphseq2 (*(   (x( ;)x))*)<#classsym> "
    . "#exname#arg(?([#targarglife])?)~#flags(?(/#private)?)(x(;~->#next)x)\n",
    "  (*(    )*)     goto #seq\n",
    "(?(<#seq>)?)#exname#arg(?([#targarglife])?)"],
   "linenoise" =>
   ["(x(;(*( )*))x)#noise#arg(?([#targarg])?)(x( ;\n)x)",
    "gt_#seq ",
    "(?(#seq)?)#noise#arg(?([#targarg])?)"],
   "debug" =>
   ["#class (#addr)\n\top_next\t\t#nextaddr\n\top_sibling\t#sibaddr\n\t"
    . "op_ppaddr\tPL_ppaddr[OP_#NAME]\n\top_type\t\t#typenum\n\top_seq\t\t"
    . "#seqnum\n\top_flags\t#flagval\n\top_private\t#privval\n"
    . "(?(\top_first\t#firstaddr\n)?)(?(\top_last\t\t#lastaddr\n)?)"
    . "(?(\top_sv\t\t#svaddr\n)?)",
    "    GOTO #addr\n",
    "#addr"],
   "env" => [$ENV{B_CONCISE_FORMAT}, $ENV{B_CONCISE_GOTO_FORMAT},
	     $ENV{B_CONCISE_TREE_FORMAT}],
  );

my($format, $gotofmt, $treefmt);
my $curcv;
my($seq_base, $cop_seq_base);

sub concise_cv {
    my ($order, $cvref) = @_;
    my $cv = svref_2object($cvref);
    $curcv = $cv;
    if ($order eq "exec") {
	walk_exec($cv->START);
    } elsif ($order eq "basic") {
	walk_topdown($cv->ROOT, sub { $_[0]->concise($_[1]) }, 0);
    } else {
	print tree($cv->ROOT, 0)
    }
}

my $start_sym = "\e(0"; # "\cN" sometimes also works
my $end_sym   = "\e(B"; # "\cO" respectively

my @tree_decorations = 
  (["  ", "--", "+-", "|-", "| ", "`-", "-", 1],
   [" ", "-", "+", "+", "|", "`", "", 0],
   ["  ", map("$start_sym$_$end_sym", "qq", "wq", "tq", "x ", "mq", "q"), 1],
   [" ", map("$start_sym$_$end_sym", "q", "w", "t", "x", "m"), "", 0],
  );
my $tree_style = 0;

my $base = 36;
my $big_endian = 1;

my $order = "basic";

sub compile {
    my @options = grep(/^-/, @_);
    my @args = grep(!/^-/, @_);
    my $do_main = 0;
    ($format, $gotofmt, $treefmt) = @{$style{"concise"}};
    for my $o (@options) {
	if ($o eq "-basic") {
	    $order = "basic";
	} elsif ($o eq "-exec") {
	    $order = "exec";
	} elsif ($o eq "-tree") {
	    $order = "tree";
	} elsif ($o eq "-compact") {
	    $tree_style |= 1;
	} elsif ($o eq "-loose") {
	    $tree_style &= ~1;
	} elsif ($o eq "-vt") {
	    $tree_style |= 2;
	} elsif ($o eq "-ascii") {
	    $tree_style &= ~2;
	} elsif ($o eq "-main") {
	    $do_main = 1;
	} elsif ($o =~ /^-base(\d+)$/) {
	    $base = $1;
	} elsif ($o eq "-bigendian") {
	    $big_endian = 1;
	} elsif ($o eq "-littleendian") {
	    $big_endian = 0;
	} elsif (exists $style{substr($o, 1)}) {
	    ($format, $gotofmt, $treefmt) = @{$style{substr($o, 1)}};
	} else {
	    warn "Option $o unrecognized";
	}
    }
    if (@args) {
	return sub {
	    for my $objname (@args) {
		$objname = "main::" . $objname unless $objname =~ /::/;
		eval "concise_cv(\$order, \\&$objname)";
		die "concise_cv($order, \\&$objname) failed: $@" if $@;
	    }
	}
    }
    if (!@args or $do_main) {
	if ($order eq "exec") {
	    return sub { return if class(main_start) eq "NULL";
			 $curcv = main_cv;
			 walk_exec(main_start) }
	} elsif ($order eq "tree") {
	    return sub { return if class(main_root) eq "NULL";
			 $curcv = main_cv;
			 print tree(main_root, 0) }
	} elsif ($order eq "basic") {
	    return sub { return if class(main_root) eq "NULL";
			 $curcv = main_cv;
			 walk_topdown(main_root,
				      sub { $_[0]->concise($_[1]) }, 0); }
	}
    }
}

my %labels;
my $lastnext;

my %opclass = ('OP' => "0", 'UNOP' => "1", 'BINOP' => "2", 'LOGOP' => "|",
	       'LISTOP' => "@", 'PMOP' => "/", 'SVOP' => "\$", 'GVOP' => "*",
	       'PVOP' => '"', 'LOOP' => "{", 'COP' => ";");

my @linenoise =
  qw'#  () sc (  @? 1  $* gv *{ m$ m@ m% m? p/ *$ $  $# & a& pt \\ s\\ rf bl
     `  *? <> ?? ?/ r/ c/ // qr s/ /c y/ =  @= C  sC Cp sp df un BM po +1 +I
     -1 -I 1+ I+ 1- I- ** *  i* /  i/ %$ i% x  +  i+ -  i- .  "  << >> <  i<
     >  i> <= i, >= i. == i= != i! <? i? s< s> s, s. s= s! s? b& b^ b| -0 -i
     !  ~  a2 si cs rd sr e^ lg sq in %x %o ab le ss ve ix ri sf FL od ch cy
     uf lf uc lc qm @  [f [  @[ eh vl ky dl ex %  ${ @{ uk pk st jn )  )[ a@
     a% sl +] -] [- [+ so rv GS GW MS MW .. f. .f && || ^^ ?: &= |= -> s{ s}
     v} ca wa di rs ;; ;  ;d }{ {  }  {} f{ it {l l} rt }l }n }r dm }g }e ^o
     ^c ^| ^# um bm t~ u~ ~d DB db ^s se ^g ^r {w }w pf pr ^O ^K ^R ^W ^d ^v
     ^e ^t ^k t. fc ic fl .s .p .b .c .l .a .h g1 s1 g2 s2 ?. l? -R -W -X -r
     -w -x -e -o -O -z -s -M -A -C -S -c -b -f -d -p -l -u -g -k -t -T -B cd
     co cr u. cm ut r. l@ s@ r@ mD uD oD rD tD sD wD cD f$ w$ p$ sh e$ k$ g3
     g4 s4 g5 s5 T@ C@ L@ G@ A@ S@ Hg Hc Hr Hw Mg Mc Ms Mr Sg Sc So rq do {e
     e} {t t} g6 G6 6e g7 G7 7e g8 G8 8e g9 G9 9e 6s 7s 8s 9s 6E 7E 8E 9E Pn
     Pu GP SP EP Gn Gg GG SG EG g0 c$ lk t$ ;s n>';

my $chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

sub op_flags {
    my($x) = @_;
    my(@v);
    push @v, "v" if ($x & 3) == 1;
    push @v, "s" if ($x & 3) == 2;
    push @v, "l" if ($x & 3) == 3;
    push @v, "K" if $x & 4;
    push @v, "P" if $x & 8;
    push @v, "R" if $x & 16;
    push @v, "M" if $x & 32;
    push @v, "S" if $x & 64;
    push @v, "*" if $x & 128;
    return join("", @v);
}

sub base_n {
    my $x = shift;
    return "-" . base_n(-$x) if $x < 0;
    my $str = "";
    do { $str .= substr($chars, $x % $base, 1) } while $x = int($x / $base);
    $str = reverse $str if $big_endian;
    return $str;
}

sub seq { return $_[0]->seq ? base_n($_[0]->seq - $seq_base) : "-" }

sub walk_topdown {
    my($op, $sub, $level) = @_;
    $sub->($op, $level);
    if ($op->flags & OPf_KIDS) {
	for (my $kid = $op->first; $$kid; $kid = $kid->sibling) {
	    walk_topdown($kid, $sub, $level + 1);
	}
    }
    if (class($op) eq "PMOP" and $ {$op->pmreplroot}
	and $op->pmreplroot->isa("B::OP")) {
	walk_topdown($op->pmreplroot, $sub, $level + 1);
    }
}

sub walklines {
    my($ar, $level) = @_;
    for my $l (@$ar) {
	if (ref($l) eq "ARRAY") {
	    walklines($l, $level + 1);
	} else {
	    $l->concise($level);
	}
    }
}

sub walk_exec {
    my($top, $level) = @_;
    my %opsseen;
    my @lines;
    my @todo = ([$top, \@lines]);
    while (@todo and my($op, $targ) = @{shift @todo}) {
	for (; $$op; $op = $op->next) {
	    last if $opsseen{$$op}++;
	    push @$targ, $op;
	    my $name = $op->name;
	    if ($name
		=~ /^(or|and|(map|grep)while|entertry|range|cond_expr)$/) {
		my $ar = [];
		push @$targ, $ar;
		push @todo, [$op->other, $ar];
	    } elsif ($name eq "subst" and $ {$op->pmreplstart}) {
		my $ar = [];
		push @$targ, $ar;
		push @todo, [$op->pmreplstart, $ar];
	    } elsif ($name =~ /^enter(loop|iter)$/) {
		$labels{$op->nextop->seq} = "NEXT";
		$labels{$op->lastop->seq} = "LAST";
		$labels{$op->redoop->seq} = "REDO";		
	    }
	}
    }
    walklines(\@lines, 0);
}

sub fmt_line {
    my($hr, $fmt, $level) = @_;
    my $text = $fmt;
    $text =~ s/\(\?\(([^\#]*?)\#(\w+)([^\#]*?)\)\?\)/
      $hr->{$2} ? $1.$hr->{$2}.$3 : ""/eg;
    $text =~ s/\(x\((.*?);(.*?)\)x\)/$order eq "exec" ? $1 : $2/egs;
    $text =~ s/\(\*\(([^;]*?)\)\*\)/$1 x $level/egs;
    $text =~ s/\(\*\((.*?);(.*?)\)\*\)/$1 x ($level - 1) . $2 x ($level>0)/egs;
    $text =~ s/#([a-zA-Z]+)(\d+)/sprintf("%-$2s", $hr->{$1})/eg;
    $text =~ s/#([a-zA-Z]+)/$hr->{$1}/eg;
    $text =~ s/[ \t]*~+[ \t]*/ /g;
    return $text;
}

my %priv;
$priv{$_}{128} = "LVINTRO"
  for ("pos", "substr", "vec", "threadsv", "gvsv", "rv2sv", "rv2hv", "rv2gv",
       "rv2av", "rv2arylen", "aelem", "helem", "aslice", "hslice", "padsv",
       "padav", "padhv");
$priv{$_}{64} = "REFC" for ("leave", "leavesub", "leavesublv", "leavewrite");
$priv{"aassign"}{64} = "COMMON";
$priv{"aassign"}{32} = "PHASH";
$priv{"sassign"}{64} = "BKWARD";
$priv{$_}{64} = "RTIME" for ("match", "subst", "substcont");
@{$priv{"trans"}}{1,2,4,8,16,64} = ("<UTF", ">UTF", "IDENT", "SQUASH", "DEL",
				    "COMPL", "GROWS");
$priv{"repeat"}{64} = "DOLIST";
$priv{"leaveloop"}{64} = "CONT";
@{$priv{$_}}{32,64,96} = ("DREFAV", "DREFHV", "DREFSV")
  for ("entersub", map("rv2${_}v", "a", "s", "h", "g"), "aelem", "helem");
$priv{"entersub"}{16} = "DBG";
$priv{"entersub"}{32} = "TARG";
@{$priv{$_}}{4,8,128} = ("INARGS","AMPER","NO()") for ("entersub", "rv2cv");
$priv{"gv"}{32} = "EARLYCV";
$priv{"aelem"}{16} = $priv{"helem"}{16} = "LVDEFER";
$priv{$_}{16} = "OURINTR" for ("gvsv", "rv2sv", "rv2av", "rv2hv", "r2gv");
$priv{$_}{16} = "TARGMY"
  for (map(($_,"s$_"),"chop", "chomp"),
       map(($_,"i_$_"), "postinc", "postdec", "multiply", "divide", "modulo",
	   "add", "subtract", "negate"), "pow", "concat", "stringify",
       "left_shift", "right_shift", "bit_and", "bit_xor", "bit_or",
       "complement", "atan2", "sin", "cos", "rand", "exp", "log", "sqrt",
       "int", "hex", "oct", "abs", "length", "index", "rindex", "sprintf",
       "ord", "chr", "crypt", "quotemeta", "join", "push", "unshift", "flock",
       "chdir", "chown", "chroot", "unlink", "chmod", "utime", "rename",
       "link", "symlink", "mkdir", "rmdir", "wait", "waitpid", "system",
       "exec", "kill", "getppid", "getpgrp", "setpgrp", "getpriority",
       "setpriority", "time", "sleep");
@{$priv{"const"}}{8,16,32,64,128} = ("STRICT","ENTERED", "$[", "BARE", "WARN");
$priv{"flip"}{64} = $priv{"flop"}{64} = "LINENUM";
$priv{"list"}{64} = "GUESSED";
$priv{"delete"}{64} = "SLICE";
$priv{"exists"}{64} = "SUB";
$priv{$_}{64} = "LOCALE"
  for ("sort", "prtf", "sprintf", "slt", "sle", "seq", "sne", "sgt", "sge",
       "scmp", "lc", "uc", "lcfirst", "ucfirst");
@{$priv{"sort"}}{1,2,4} = ("NUM", "INT", "REV");
$priv{"threadsv"}{64} = "SVREFd";
$priv{$_}{16} = "INBIN" for ("open", "backtick");
$priv{$_}{32} = "INCR" for ("open", "backtick");
$priv{$_}{64} = "OUTBIN" for ("open", "backtick");
$priv{$_}{128} = "OUTCR" for ("open", "backtick");
$priv{"exit"}{128} = "VMS";

sub private_flags {
    my($name, $x) = @_;
    my @s;
    for my $flag (128, 96, 64, 32, 16, 8, 4, 2, 1) {
	if ($priv{$name}{$flag} and $x & $flag and $x >= $flag) {
	    $x -= $flag;
	    push @s, $priv{$name}{$flag};
	}
    }
    push @s, $x if $x;
    return join(",", @s);
}

sub concise_op {
    my ($op, $level, $format) = @_;
    my %h;
    $h{exname} = $h{name} = $op->name;
    $h{NAME} = uc $h{name};
    $h{class} = class($op);
    $h{extarg} = $h{targ} = $op->targ;
    $h{extarg} = "" unless $h{extarg};
    if ($h{name} eq "null" and $h{targ}) {
	$h{exname} = "ex-" . substr(ppname($h{targ}), 3);
	$h{extarg} = "";
    } elsif ($h{targ}) {
	my $padname = (($curcv->PADLIST->ARRAY)[0]->ARRAY)[$h{targ}];
	if (defined $padname and class($padname) ne "SPECIAL") {
	    $h{targarg}  = $padname->PVX;
	    my $intro = $padname->NVX - $cop_seq_base;
	    my $finish = int($padname->IVX) - $cop_seq_base;
	    $finish = "end" if $finish == 999999999 - $cop_seq_base;
	    $h{targarglife} = "$h{targarg}:$intro,$finish";
	} else {
	    $h{targarglife} = $h{targarg} = "t" . $h{targ};
	}
    }
    $h{arg} = "";
    $h{svclass} = $h{svaddr} = $h{svval} = "";
    if ($h{class} eq "PMOP") {
	my $precomp = $op->precomp;
	$precomp = defined($precomp) ? "/$precomp/" : "";
	my $pmreplroot = $op->pmreplroot;
	my ($pmreplroot, $pmreplstart);
	if ($ {$pmreplroot = $op->pmreplroot} && $pmreplroot->isa("B::GV")) {
	    # with C<@stash_array = split(/pat/, str);>,
	    #  *stash_array is stored in pmreplroot.
	    $h{arg} = "($precomp => \@" . $pmreplroot->NAME . ")";
	} elsif ($ {$op->pmreplstart}) {
	    undef $lastnext;
	    $pmreplstart = "replstart->" . seq($op->pmreplstart);
	    $h{arg} = "(" . join(" ", $precomp, $pmreplstart) . ")";
	} else {
	    $h{arg} = "($precomp)";
	}
    } elsif ($h{class} eq "PVOP" and $h{name} ne "trans") {
	$h{arg} = '("' . $op->pv . '")';
	$h{svval} = '"' . $op->pv . '"';
    } elsif ($h{class} eq "COP") {
	my $label = $op->label;
	$h{coplabel} = $label;
	$label = $label ? "$label: " : "";
	my $loc = $op->file;
	$loc =~ s[.*/][];
	$loc .= ":" . $op->line;
	my($stash, $cseq) = ($op->stash->NAME, $op->cop_seq - $cop_seq_base);
	my $arybase = $op->arybase;
	$arybase = $arybase ? ' $[=' . $arybase : "";
	$h{arg} = "($label$stash $cseq $loc$arybase)";
    } elsif ($h{class} eq "LOOP") {
	$h{arg} = "(next->" . seq($op->nextop) . " last->" . seq($op->lastop)
	  . " redo->" . seq($op->redoop) . ")";
    } elsif ($h{class} eq "LOGOP") {
	undef $lastnext;
	$h{arg} = "(other->" . seq($op->other) . ")";
    } elsif ($h{class} eq "SVOP") {
	my $sv = $op->sv;
	$h{svclass} = class($sv);
	$h{svaddr} = sprintf("%#x", $$sv);
	if ($h{svclass} eq "GV") {
	    my $gv = $sv;
	    my $stash = $gv->STASH->NAME;
	    if ($stash eq "main") {
		$stash = "";
	    } else {
		$stash = $stash . "::";
	    }
	    $h{arg} = "(*$stash" . $gv->SAFENAME . ")";
	    $h{svval} = "*$stash" . $gv->SAFENAME;
	} else {
	    while (class($sv) eq "RV") {
		$h{svval} .= "\\";
		$sv = $sv->RV;
	    }
	    if (class($sv) eq "SPECIAL") {
		$h{svval} = ["Null", "sv_undef", "sv_yes", "sv_no"]->[$$sv];
	    } elsif ($sv->FLAGS & SVf_NOK) {
		$h{svval} = $sv->NV;
	    } elsif ($sv->FLAGS & SVf_IOK) {
		$h{svval} = $sv->IV;
	    } elsif ($sv->FLAGS & SVf_POK) {
		$h{svval} = cstring($sv->PV);
	    }
	    $h{arg} = "($h{svclass} $h{svval})";
	}
    }
    $h{seq} = $h{hyphseq} = seq($op);
    $h{seq} = "" if $h{seq} eq "-";
    $h{seqnum} = $op->seq;
    $h{next} = $op->next;
    $h{next} = (class($h{next}) eq "NULL") ? "(end)" : seq($h{next});
    $h{nextaddr} = sprintf("%#x", $ {$op->next});
    $h{sibaddr} = sprintf("%#x", $ {$op->sibling});
    $h{firstaddr} = sprintf("%#x", $ {$op->first}) if $op->can("first");
    $h{lastaddr} = sprintf("%#x", $ {$op->last}) if $op->can("last");

    $h{classsym} = $opclass{$h{class}};
    $h{flagval} = $op->flags;
    $h{flags} = op_flags($op->flags);
    $h{privval} = $op->private;
    $h{private} = private_flags($h{name}, $op->private);
    $h{addr} = sprintf("%#x", $$op);
    $h{label} = $labels{$op->seq};
    $h{typenum} = $op->type;
    $h{noise} = $linenoise[$op->type];
    return fmt_line(\%h, $format, $level);
}

sub B::OP::concise {
    my($op, $level) = @_;
    if ($order eq "exec" and $lastnext and $$lastnext != $$op) {
	my $h = {"seq" => seq($lastnext), "class" => class($lastnext),
		 "addr" => sprintf("%#x", $$lastnext)};
	print fmt_line($h, $gotofmt, $level+1);
    }
    $lastnext = $op->next;
    print concise_op($op, $level, $format);
}

sub tree {
    my $op = shift;
    my $level = shift;
    my $style = $tree_decorations[$tree_style];
    my($space, $single, $kids, $kid, $nokid, $last, $lead, $size) = @$style;
    my $name = concise_op($op, $level, $treefmt);
    if (not $op->flags & OPf_KIDS) {
	return $name . "\n";
    }
    my @lines;
    for (my $kid = $op->first; $$kid; $kid = $kid->sibling) {
	push @lines, tree($kid, $level+1);
    }
    my $i;
    for ($i = $#lines; substr($lines[$i], 0, 1) eq " "; $i--) {
	$lines[$i] = $space . $lines[$i];
    }
    if ($i > 0) {
	$lines[$i] = $last . $lines[$i];
	while ($i-- > 1) {
	    if (substr($lines[$i], 0, 1) eq " ") {
		$lines[$i] = $nokid . $lines[$i];
	    } else {
		$lines[$i] = $kid . $lines[$i];		
	    }
	}
	$lines[$i] = $kids . $lines[$i];
    } else {
	$lines[0] = $single . $lines[0];
    }
    return("$name$lead" . shift @lines,
           map(" " x (length($name)+$size) . $_, @lines));
}

# This is a bit of a hack; the 2 and 15 were determined empirically.
# These need to stay the last things in the module.
$cop_seq_base = svref_2object(eval 'sub{0;}')->START->cop_seq + 2;
$seq_base = svref_2object(eval 'sub{}')->START->seq + 15;

1;

__END__

=head1 NAME

B::Concise - Walk Perl syntax tree, printing concise info about ops

=head1 SYNOPSIS

    perl -MO=Concise[,OPTIONS] foo.pl

=head1 DESCRIPTION

This compiler backend prints the internal OPs of a Perl program's syntax
tree in one of several space-efficient text formats suitable for debugging
the inner workings of perl or other compiler backends. It can print OPs in
the order they appear in the OP tree, in the order they will execute, or
in a text approximation to their tree structure, and the format of the
information displyed is customizable. Its function is similar to that of
perl's B<-Dx> debugging flag or the B<B::Terse> module, but it is more
sophisticated and flexible.

=head1 OPTIONS

Arguments that don't start with a hyphen are taken to be the names of
subroutines to print the OPs of; if no such functions are specified, the
main body of the program (outside any subroutines, and not including use'd
or require'd files) is printed.

=over 4

=item B<-basic>

Print OPs in the order they appear in the OP tree (a preorder
traversal, starting at the root). The indentation of each OP shows its
level in the tree.  This mode is the default, so the flag is included
simply for completeness.

=item B<-exec>

Print OPs in the order they would normally execute (for the majority
of constructs this is a postorder traversal of the tree, ending at the
root). In most cases the OP that usually follows a given OP will
appear directly below it; alternate paths are shown by indentation. In
cases like loops when control jumps out of a linear path, a 'goto'
line is generated.

=item B<-tree>

Print OPs in a text approximation of a tree, with the root of the tree
at the left and 'left-to-right' order of children transformed into
'top-to-bottom'. Because this mode grows both to the right and down,
it isn't suitable for large programs (unless you have a very wide
terminal).

=item B<-compact>

Use a tree format in which the minimum amount of space is used for the
lines connecting nodes (one character in most cases). This squeezes out
a few precious columns of screen real estate.

=item B<-loose>

Use a tree format that uses longer edges to separate OP nodes. This format
tends to look better than the compact one, especially in ASCII, and is
the default.

=item B<-vt>

Use tree connecting characters drawn from the VT100 line-drawing set.
This looks better if your terminal supports it.

=item B<-ascii>

Draw the tree with standard ASCII characters like C<+> and C<|>. These don't
look as clean as the VT100 characters, but they'll work with almost any
terminal (or the horizontal scrolling mode of less(1)) and are suitable
for text documentation or email. This is the default.

=item B<-main>

Include the main program in the output, even if subroutines were also
specified.

=item B<-base>I<n>

Print OP sequence numbers in base I<n>. If I<n> is greater than 10, the
digit for 11 will be 'a', and so on. If I<n> is greater than 36, the digit
for 37 will be 'A', and so on until 62. Values greater than 62 are not
currently supported. The default is 36.

=item B<-bigendian>

Print sequence numbers with the most significant digit first. This is the
usual convention for Arabic numerals, and the default.

=item B<-littleendian>

Print seqence numbers with the least significant digit first.

=item B<-concise>

Use the author's favorite set of formatting conventions. This is the
default, of course.

=item B<-terse>

Use formatting conventions that emulate the ouput of B<B::Terse>. The
basic mode is almost indistinguishable from the real B<B::Terse>, and the
exec mode looks very similar, but is in a more logical order and lacks
curly brackets. B<B::Terse> doesn't have a tree mode, so the tree mode
is only vaguely reminiscient of B<B::Terse>.

=item B<-linenoise>

Use formatting conventions in which the name of each OP, rather than being
written out in full, is represented by a one- or two-character abbreviation.
This is mainly a joke.

=item B<-debug>

Use formatting conventions reminiscient of B<B::Debug>; these aren't
very concise at all.

=item B<-env>

Use formatting conventions read from the environment variables
C<B_CONCISE_FORMAT>, C<B_CONCISE_GOTO_FORMAT>, and C<B_CONCISE_TREE_FORMAT>.

=back

=head1 FORMATTING SPECIFICATIONS

For each general style ('concise', 'terse', 'linenoise', etc.) there are
three specifications: one of how OPs should appear in the basic or exec
modes, one of how 'goto' lines should appear (these occur in the exec
mode only), and one of how nodes should appear in tree mode. Each has the
same format, described below. Any text that doesn't match a special
pattern is copied verbatim.

=over 4

=item B<(x(>I<exec_text>B<;>I<basic_text>B<)x)>

Generates I<exec_text> in exec mode, or I<basic_text> in basic mode.

=item B<(*(>I<text>B<)*)>

Generates one copy of I<text> for each indentation level.

=item B<(*(>I<text1>B<;>I<text2>B<)*)>

Generates one fewer copies of I<text1> than the indentation level, followed
by one copy of I<text2> if the indentation level is more than 0.

=item B<(?(>I<text1>B<#>I<var>I<Text2>B<)?)>

If the value of I<var> is true (not empty or zero), generates the
value of I<var> surrounded by I<text1> and I<Text2>, otherwise
nothing.

=item B<#>I<var>

Generates the value of the variable I<var>.

=item B<#>I<var>I<N>

Generates the value of I<var>, left jutified to fill I<N> spaces.

=item B<~>

Any number of tildes and surrounding whitespace will be collapsed to
a single space.

=back

The following variables are recognized:

=over 4

=item B<#addr>

The address of the OP, in hexidecimal.

=item B<#arg>

The OP-specific information of the OP (such as the SV for an SVOP, the
non-local exit pointers for a LOOP, etc.) enclosed in paretheses.

=item B<#class>

The B-determined class of the OP, in all caps.

=item B<#classym>

A single symbol abbreviating the class of the OP.

=item B<#coplabel>

The label of the statement or block the OP is the start of, if any.

=item B<#exname>

The name of the OP, or 'ex-foo' if the OP is a null that used to be a foo.

=item B<#extarg>

The target of the OP, or nothing for a nulled OP.

=item B<#firstaddr>

The address of the OP's first child, in hexidecimal.

=item B<#flags>

The OP's flags, abbreviated as a series of symbols.

=item B<#flagval>

The numeric value of the OP's flags.

=item B<#hyphenseq>

The sequence number of the OP, or a hyphen if it doesn't have one.

=item B<#label>

'NEXT', 'LAST', or 'REDO' if the OP is a target of one of those in exec
mode, or empty otherwise.

=item B<#lastaddr>

The address of the OP's last child, in hexidecimal.

=item B<#name>

The OP's name.

=item B<#NAME>

The OP's name, in all caps.

=item B<#next>

The sequence number of the OP's next OP.

=item B<#nextaddr>

The address of the OP's next OP, in hexidecimal.

=item B<#noise>

The two-character abbreviation for the OP's name.

=item B<#private>

The OP's private flags, rendered with abbreviated names if possible.

=item B<#privval>

The numeric value of the OP's private flags.

=item B<#seq>

The sequence number of the OP.

=item B<#seqnum>

The real sequence number of the OP, as a regular number and not adjusted
to be relative to the start of the real program. (This will generally be
a fairly large number because all of B<B::Concise> is compiled before
your program is).

=item B<#sibaddr>

The address of the OP's next youngest sibling, in hexidecimal.

=item B<#svaddr>

The address of the OP's SV, if it has an SV, in hexidecimal.

=item B<#svclass>

The class of the OP's SV, if it has one, in all caps (e.g., 'IV').

=item B<#svval>

The value of the OP's SV, if it has one, in a short human-readable format.

=item B<#targ>

The numeric value of the OP's targ.

=item B<#targarg>

The name of the variable the OP's targ refers to, if any, otherwise the
letter t followed by the OP's targ in decimal.

=item B<#targarglife>

Same as B<#targarg>, but followed by the COP sequence numbers that delimit
the variable's lifetime (or 'end' for a variable in an open scope) for a
variable.

=item B<#typenum>

The numeric value of the OP's type, in decimal.

=back

=head1 ABBREVIATIONS

=head2 OP flags abbreviations

    v      OPf_WANT_VOID    Want nothing (void context)
    s      OPf_WANT_SCALAR  Want single value (scalar context)
    l      OPf_WANT_LIST    Want list of any length (list context)
    K      OPf_KIDS         There is a firstborn child.
    P      OPf_PARENS       This operator was parenthesized.
                             (Or block needs explicit scope entry.)
    R      OPf_REF          Certified reference.
                             (Return container, not containee).
    M      OPf_MOD          Will modify (lvalue).
    S      OPf_STACKED      Some arg is arriving on the stack.
    *      OPf_SPECIAL      Do something weird for this op (see op.h)

=head2 OP class abbreviations

    0      OP (aka BASEOP)  An OP with no children
    1      UNOP             An OP with one child
    2      BINOP            An OP with two children
    |      LOGOP            A control branch OP
    @      LISTOP           An OP that could have lots of children
    /      PMOP             An OP with a regular expression
    $      SVOP             An OP with an SV
    "      PVOP             An OP with a string
    {      LOOP             An OP that holds pointers for a loop
    ;      COP              An OP that marks the start of a statement

=head1 AUTHOR

Stephen McCamant, C<smcc@CSUA.Berkeley.EDU>

=cut
