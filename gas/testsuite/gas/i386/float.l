.*: Assembler messages:
.*:3: Warning:.*faddp.*
.*:14: Warning:.*fsubp.*
.*:25: Warning:.*fsubrp.*
.*:36: Warning:.*fmulp.*
.*:47: Warning:.*fdivp.*
.*:58: Warning:.*fdivrp.*
   1 [ 	]*.psize 0
   2 [ 	]*.text
   3 0000 DEC1 [ 	]*fadd
.*Warning:.*faddp.*
   4 0002 D8C3 [ 	]*fadd	%st\(3\)
   5 0004 D8C3 [ 	]*fadd	%st\(3\),%st
   6 0006 DCC3 [ 	]*fadd	%st,%st\(3\)
   7 0008 D803 [ 	]*fadds	\(%ebx\)
   8 000a DC03 [ 	]*faddl	\(%ebx\)
   9 000c DE03 [ 	]*fiadds	\(%ebx\)
  10 000e DA03 [ 	]*fiaddl	\(%ebx\)
  11 0010 DEC1 [ 	]*faddp
  12 0012 DEC3 [ 	]*faddp	%st\(3\)
  13 0014 DEC3 [ 	]*faddp	%st,%st\(3\)
  14 0016 DEE1 [ 	]*fsub
.*Warning:.*fsubp.*
  15 0018 D8E3 [ 	]*fsub	%st\(3\)
  16 001a D8E3 [ 	]*fsub	%st\(3\),%st
  17 001c DCE3 [ 	]*fsub	%st,%st\(3\)
  18 001e D823 [ 	]*fsubs	\(%ebx\)
  19 0020 DC23 [ 	]*fsubl	\(%ebx\)
  20 0022 DE23 [ 	]*fisubs	\(%ebx\)
  21 0024 DA23 [ 	]*fisubl	\(%ebx\)
  22 0026 DEE1 [ 	]*fsubp
  23 0028 DEE3 [ 	]*fsubp	%st\(3\)
  24 002a DEE3 [ 	]*fsubp	%st,%st\(3\)
  25 002c DEE9 [ 	]*fsubr
.*Warning:.*fsubrp.*
  26 002e D8EB [ 	]*fsubr	%st\(3\)
  27 0030 D8EB [ 	]*fsubr	%st\(3\),%st
  28 0032 DCEB [ 	]*fsubr	%st,%st\(3\)
  29 0034 D82B [ 	]*fsubrs	\(%ebx\)
  30 0036 DC2B [ 	]*fsubrl	\(%ebx\)
  31 0038 DE2B [ 	]*fisubrs	\(%ebx\)
  32 003a DA2B [ 	]*fisubrl	\(%ebx\)
  33 003c DEE9 [ 	]*fsubrp
  34 003e DEEB [ 	]*fsubrp	%st\(3\)
  35 0040 DEEB [ 	]*fsubrp	%st,%st\(3\)
  36 0042 DEC9 [ 	]*fmul
.*Warning:.*fmulp.*
  37 0044 D8CB [ 	]*fmul	%st\(3\)
  38 0046 D8CB [ 	]*fmul	%st\(3\),%st
  39 0048 DCCB [ 	]*fmul	%st,%st\(3\)
  40 004a D80B [ 	]*fmuls	\(%ebx\)
  41 004c DC0B [ 	]*fmull	\(%ebx\)
  42 004e DE0B [ 	]*fimuls	\(%ebx\)
  43 0050 DA0B [ 	]*fimull	\(%ebx\)
  44 0052 DEC9 [ 	]*fmulp
  45 0054 DECB [ 	]*fmulp	%st\(3\)
  46 0056 DECB [ 	]*fmulp	%st,%st\(3\)
  47 0058 DEF1 [ 	]*fdiv
.*Warning:.*fdivp.*
  48 005a D8F3 [ 	]*fdiv	%st\(3\)
  49 005c D8F3 [ 	]*fdiv	%st\(3\),%st
  50 005e DCF3 [ 	]*fdiv	%st,%st\(3\)
  51 0060 D833 [ 	]*fdivs	\(%ebx\)
  52 0062 DC33 [ 	]*fdivl	\(%ebx\)
  53 0064 DE33 [ 	]*fidivs	\(%ebx\)
  54 0066 DA33 [ 	]*fidivl	\(%ebx\)
  55 0068 DEF1 [ 	]*fdivp
  56 006a DEF3 [ 	]*fdivp	%st\(3\)
  57 006c DEF3 [ 	]*fdivp	%st,%st\(3\)
  58 006e DEF9 [ 	]*fdivr
.*Warning:.*fdivrp.*
  59 0070 D8FB [ 	]*fdivr	%st\(3\)
  60 0072 D8FB [ 	]*fdivr	%st\(3\),%st
  61 0074 DCFB [ 	]*fdivr	%st,%st\(3\)
  62 0076 D83B [ 	]*fdivrs	\(%ebx\)
  63 0078 DC3B [ 	]*fdivrl	\(%ebx\)
  64 007a DE3B [ 	]*fidivrs	\(%ebx\)
  65 007c DA3B [ 	]*fidivrl	\(%ebx\)
  66 007e DEF9 [ 	]*fdivrp
  67 0080 DEFB [ 	]*fdivrp	%st\(3\)
  68 0082 DEFB [ 	]*fdivrp	%st,%st\(3\)
  69 [ 	]*
  70 0084 00000000 [ 	]*.p2align	4,0
  70      00000000 
  70      00000000 
