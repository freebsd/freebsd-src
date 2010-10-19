	.text
	.global push
push:
	[--Sp] = syscfg;
	[--SP] = Lc0;
	[--sp] = R7;
	[--sp] = A0.W;
	[--sP] = Cycles;
	[--Sp] = b2;
	[--sp] = m1;
	[--SP] = P0;

	.text
	.global push_multiple
push_multiple:
	[--sp] = (r7:2, p5:0);
	[--SP] = (R7:6);
	[--Sp] = (p5:2);

	.text
	.global pop
pop:
	usp = [ Sp++];
	Reti = [sp++];
	i0 = [sp++];
	Seqstat = [sp++];
	L2 = [sp++];
	R5 = [SP ++ ];
	Fp = [Sp ++];

	.text
	.global pop_multiple
pop_multiple:
	(R7:5, P5:0) = [sp++];
	(r7:6) = [SP++];
	(P5:4) = [Sp++];

	.text
	.global link
link:
	link 8;
	link 0x3fffc;
	link 0x20004;

	.text
	.global unlink
unlink:
	unlink;
	
