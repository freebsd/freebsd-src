
	.text
func:
	.set noreorder
	li.s	$f1, 1.0	
	li.s	$f3, 1.9	
	add.s	$f5, $f1, $f3
	cvt.d.s	$f8,$f7
	cvt.d.w	$f8,$f7
	cvt.s.d	$f7,$f8
	trunc.w.d $f7,$f8

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
