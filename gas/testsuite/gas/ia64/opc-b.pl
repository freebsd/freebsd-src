@ph = ( "", ".few", ".many" );
@bwh = ( ".sptk", ".spnt", ".dptk", ".dpnt" );
@dh = ( "", ".clr" );

@iprel = ( ".cond", ".wexit", ".wtop", ".cloop", ".cexit", ".ctop", ".call" );
@indir = ( ".cond", ".ia", ".ret", ".call" );
%noqual = ( ".ia", 1, ".cloop", 1, ".ctop", 1, ".cexit", 1 );
%slottwo = ( ".cloop", 1, ".ctop", 1, ".cexit", 1, ".wtop", 1, ".wexit", 1 );

print ".L0:\n\n";

foreach $i (@iprel) {
  $call = ($i eq ".call" ? "b0 = " : "");
  foreach $b (@bwh) {
    foreach $p (@ph) {
      foreach $d (@dh) {
	if ($slottwo{$i}) {
	  if (!$noqual{$i}) {
	    print ("\t{ .bbb; (p2) br${i}${b}${p}${d} ${call}.L1 ;; }\n");
	  }
	  print ("\t{ .bbb; br${i}${b}${p}${d} ${call}.L1 ;; }\n");
	} else {
	  print ("\t{ .bbb; nop.b 0\n");
	  if (!$noqual{$i}) {
	    print ("(p2)\tbr${i}${b}${p}${d} ${call}.L1\n");
	  } else {
	    print ("\tnop.b 0\n");
	  }
	  print ("\tbr${i}${b}${p}${d} ${call}.L0\n");
	  print ("\t;; }\n");
	}
      }
    }
  }
  print "\n";
}

foreach $i (@indir) {
  $call = ($i eq ".call" ? "b0 = " : "");
  foreach $b (@bwh) {
    foreach $p (@ph) {
      foreach $d (@dh) {
	print ("\t{ .bbb; nop.b 0;\n");
	if (!$noqual{$i}) {
	  print ("(p2)\tbr${i}${b}${p}${d} ${call}b2\n");
	} else {
	  print ("\tnop.b 0\n");
	}
	print ("\tbr${i}${b}${p}${d} ${call}b2\n");
	print ("\t;; }\n");
      }
    }
  }
  print "\n";
}

@ih = ( "", ".imp" );
@ipwh = ( ".sptk", ".loop", ".dptk", ".exit" );
@indwh = ( ".sptk", ".dptk" );

$CTR = 2;

foreach $w (@ipwh) {
  foreach $i (@ih) {
    print ("\t{ .bbb; break.b 0; nop.b 0\n");
    print ("\tbrp${w}${i} .L0, .L${CTR}\n");
    print ("\t;; }\n");
  }
  print (".L${CTR}:\n");
  ++$CTR;
}

print "\n";

foreach $b ("", ".ret") {
  foreach $w (@indwh) {
    foreach $i (@ih) {
      print ("\t{ .bbb; break.b 0; nop.b 0\n");
      print ("\tbrp${b}${w}${i} b3, .L${CTR}\n");
      print ("\t;; }\n");
    }
    print (".L${CTR}:\n");
    ++$CTR;
  }
  print "\n";
}

print ".space 5888\n";

@last = ( "cover", "clrrrb", "clrrrb.pr", "rfi", "bsw.0", "bsw.1", "epc" );
foreach $i (@last) {
  print "\t{ .bbb; nop.b 0; nop.b 0; $i ;; }\n";
}

print "\n.L1:\n";
