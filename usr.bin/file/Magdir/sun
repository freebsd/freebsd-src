#
# Values for big-endian Sun (MC680x0, SPARC) binaries on pre-5.x
# releases.
# (5.x uses ELF.)
#
0	belong&077777777	0600413		sparc demand paged
>0	byte		&0x80
>>20	belong		<4096		shared library
>>20	belong		=4096		dynamically linked executable
>>20	belong		>4096		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped
0	belong&077777777	0600410		sparc pure
>0	byte		&0x80		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped
0	belong&077777777	0600407		sparc
>0	byte		&0x80		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped

0	belong&077777777	0400413		mc68020 demand paged
>0	byte		&0x80
>>20	belong		<4096		shared library
>>20	belong		=4096		dynamically linked executable
>>20	belong		>4096		dynamically linked executable
>16	belong		>0		not stripped
0	belong&077777777	0400410		mc68020 pure
>0	byte		&0x80		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped
0	belong&077777777	0400407		mc68020
>0	byte		&0x80		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped

0	belong&077777777	0200413		mc68010 demand paged
>0	byte		&0x80
>>20	belong		<4096		shared library
>>20	belong		=4096		dynamically linked executable
>>20	belong		>4096		dynamically linked executable
>16	belong		>0		not stripped
0	belong&077777777	0200410		mc68010 pure
>0	byte		&0x80		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped
0	belong&077777777	0200407		mc68010
>0	byte		&0x80		dynamically linked executable
>0	byte		^0x80		executable
>16	belong		>0		not stripped

# reworked these to avoid anything beginning with zero becoming "old sun-2"
0	belong		0407		old sun-2 executable
>16	belong		>0		not stripped
0	belong		0410		old sun-2 pure executable
>16	belong		>0		not stripped
0	belong		0413		old sun-2 demand paged executable
>16	belong		>0		not stripped

#
# Core files.  "SPARC 4.x BCP" means "core file from a SunOS 4.x SPARC
# binary executed in compatibility mode under SunOS 5.x".
#
0	belong		0x080456	SunOS core file
>4	belong		432		(SPARC)
>>132	string		>\0		from '%s'
>>116	belong		=3		(quit)
>>116	belong		=4		(illegal instruction)
>>116	belong		=5		(trace trap)
>>116	belong		=6		(abort)
>>116	belong		=7		(emulator trap)
>>116	belong		=8		(arithmetic exception)
>>116	belong		=9		(kill)
>>116	belong		=10		(bus error)
>>116	belong		=11		(segmentation violation)
>>116	belong		=12		(bad argument to system call)
>>116	belong		=29		(resource lost)
>>120	belong		x		(T=%dK,
>>124	belong		x		D=%dK,
>>128	belong		x		S=%dK)
>4	belong		826		(68K)
>>128	string		>\0		from '%s'
>4	belong		456		(SPARC 4.x BCP)
>>152	string		>\0		from '%s'
