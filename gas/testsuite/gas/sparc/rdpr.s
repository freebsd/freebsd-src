# Test rdpr
	.text
	rdpr %tpc,%g1
	rdpr %tnpc,%g2
	rdpr %tstate,%g3
	rdpr %tt,%g4
	rdpr %tick,%g5
	rdpr %tba,%g6
	rdpr %pstate,%g7
	rdpr %tl,%o0
	rdpr %pil,%o1
	rdpr %cwp,%o2
	rdpr %cansave,%o3
	rdpr %canrestore,%o4
	rdpr %cleanwin,%o5
	rdpr %otherwin,%o6
	rdpr %wstate,%o7
	rdpr %fq,%l0
	rdpr %ver,%l1
