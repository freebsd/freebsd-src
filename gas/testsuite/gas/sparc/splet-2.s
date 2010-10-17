! Test sparclet coprocessor registers.

	.text
	.global start
start:

	cwrcxt    %g1,%ccsr
	cwrcxt    %g1,%ccfr
	cwrcxt    %g1,%cccrcr
	cwrcxt    %g1,%ccpr
	cwrcxt    %g1,%ccsr2
	cwrcxt    %g1,%cccrr
	cwrcxt    %g1,%ccrstr

	crdcxt    %ccsr,%g1
	crdcxt    %ccfr,%g1
	crdcxt    %cccrcr,%g1
	crdcxt    %ccpr,%g1
	crdcxt    %ccsr2,%g1
	crdcxt    %cccrr,%g1
	crdcxt    %ccrstr,%g1
