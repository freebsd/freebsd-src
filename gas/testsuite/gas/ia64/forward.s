two == 2*one
one = 1
three == 3*one
four = 4*one

RA == rA
rA = r2

PA == pA
pA = p6

 .text
_start:
	alloc	r31 = one + 1, two + 2, three + 3, four + 4
	dep.z	RA = one, two + 3, three + 4
(PA)	br.sptk	_start
	;;

one = -1
rA = r3
pA = p7

.L1:
	alloc	r31 = one + 1, two + 2, three + 3, four - 4
	dep.z	RA = one, two + 3, three + 4
(PA)	br.sptk	.L1
	;;
