# Test wrpr
	.text
	wrpr %g1,%tpc
	wrpr %g2,%tnpc
	wrpr %g3,%tstate
	wrpr %g4,%tt
	wrpr %g5,%tick
	wrpr %g6,%tba
	wrpr %g7,%pstate
	wrpr %o0,%tl
	wrpr %o1,%pil
	wrpr %o2,%cwp
	wrpr %o3,%cansave
	wrpr %o4,%canrestore
	wrpr %o5,%cleanwin
	wrpr %o6,%otherwin
	wrpr %o7,%wstate
