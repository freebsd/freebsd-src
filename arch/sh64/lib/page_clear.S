/* Written by Richard P. Curnow, SuperH (UK) Ltd.

   Tight version of memset for the case of just clearing a page.  It turns out
   that having the alloco's spaced out slightly due to the increment/branch
   pair causes them to contend less for access to the cache.  Similarly,
   keeping the stores apart from the allocos causes less contention.  => Do two
   separate loops.  Do multiple stores per loop to amortise the
   increment/branch cost a little.

   Parameters:
   r2 : source effective address (start of page)

   Always clears 4096 bytes.
  
*/

	.section .text..SHmedia32,"ax"
	.little

	.balign 8
	.global sh64_page_clear
sh64_page_clear:
	pta/l 1f, tr1
	pta/l 2f, tr2
	ptabs/l r18, tr0

	movi 4096, r7
	add  r2, r7, r7
	add  r2, r63, r6
1:
	alloco r6, 0
	addi	r6, 32, r6
	bgt/l	r7, r6, tr1

	add  r2, r63, r6
2:
	st.q  r6,   0, r63
	st.q  r6,   8, r63
	st.q  r6,  16, r63
	st.q  r6,  24, r63
	addi r6, 32, r6
	bgt/l r7, r6, tr2
	
	blink tr0, r63


