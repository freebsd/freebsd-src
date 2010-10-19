	.text
	.global prefetch
prefetch:
	prefetch[p5];
	PreFetch [fp++];
	PREFETCH [SP];

	.text
	.global flush
flush:
	flush[ p2 ];
	FLUsH [SP++];

	.text
	.global flushinv
flushinv:
	flushinv[ P4 ++ ];
	FLUshINv [ fp ];

	.text
	.global iflush
iflush:
	iflush[ p3 ];
	iflush [ fp++ ];
