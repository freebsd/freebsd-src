//
// Verify DV detection on branch variations
//			
.text
	.explicit
	// example from rth
3:		
	{ .mib
(p6)	  ld8 gp = [ret0]
(p6)	  mov b6 = r2
(p6)	  br.call.sptk.many b0 = b6 // if taken, clears b6/r2 usage
	}
	{ .mib
	  mov gp = r30
(p6)	  br.sptk.few 3b
	}  
