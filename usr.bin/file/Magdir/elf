#
# ELF
# Missing MIPS image type and flags
#
# Question marks on processor types flag "should not happen because the
# byte order is wrong".  We have to check the byte order flag to see what
# byte order all the other stuff in the header is in.
#
0	string		\177ELF		ELF
>4	byte		0		invalid class
>4	byte		1		32-bit
>4	byte		2		64-bit
>5	byte		0		invalid byte order
>5	byte		1		LSB
>>16	leshort		0		unknown type
>>16	leshort		1		relocatable
>>16	leshort		2		executable
>>16	leshort		3		dynamic lib
>>16	leshort		4		core file
>>18	leshort		0		unknown machine
>>18	leshort		1		WE32100 and up
>>18	leshort		2		SPARC?
>>18	leshort		3		i386 (386 and up)
>>18	leshort		4		M68000?
>>18	leshort		5		M88000?
>>18	leshort		7		i860
>>20	lelong		1		Version 1
>>36	lelong		1		MathCoPro/FPU/MAU Required
>5	byte		2		MSB
>>16	beshort		0		unknown type
>>16	beshort		1		relocatable
>>16	beshort		2		executable
>>16	beshort		3		dynamic lib
>>16	beshort		4		core file
>>18	beshort		0		unknown machine
>>18	beshort		1		WE32100 and up
>>18	beshort		2		SPARC
>>18	beshort		3		i386 (386 and up)?
>>18	beshort		4		M68000
>>18	beshort		5		M88000
>>18	beshort		7		i860
>>20	belong		1		Version 1
>>36	belong		1		MathCoPro/FPU/MAU Required

