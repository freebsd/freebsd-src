_divert__(-1)
<$Id: none.m4,v 1.1 1993/11/03 00:55:39 paul Exp $>

Switches:

_define__(<_ALL_ARCH__>,<0>)           (Meant as most inclusive; file turning 
					it on is expected to also turn on
					all arch-related switches including
					"_GENERIC__")
_define__(<_GENERIC__>,<1>)            (may not be quite all configs; 
					meant for "most vanilla" manual)
_define__(<_INTERNALS__>,<0>)

_define__(<_AOUT__>,<1>)		Object formats.  Note we turn on one.
_define__(<_BOUT__>,<0>)
_define__(<_COFF__>,<0>)
_define__(<_ELF__>,<0>)

                                        Properties of the assembler
_define__(<_DIFFTABKLUG__>,1)           Do we use the difference-table kluge?
_define__(<_IEEEFLOAT__>,0)             IEEE floating-point?
_define__(<_W32__>,0)                   word is 32 bits
_define__(<_W16__>,1)                   word is 16 bits

_define__(<_A29K__>,<0>)		Specific architectures.  Note none
_define__(<_H8__>,<0>)		        starts out on.
_define__(<_I80386__>,<0>)
_define__(<_I960__>,<0>)
_define__(<_M680X0__>,<0>)
_define__(<_SPARC__>,<0>)
_define__(<_VAX__>,<0>)
_define__(<_VXWORKS__>,<0>)

Text:

Default names; individual configs may override
Assembler:
_define__(<_AS__>,<as>)
C Compiler:
_define__(<_GCC__>,<gcc>)
Linker:
_define__(<_LD__>,<ld>)
Debugger name:
_define__(<_GDBN__>,<GDB>)
Debugger program:
_define__(<_GDBP__>,<gdb>)
Debugger init file:
_define__(<_GDBINIT__>,<.gdbinit>)

Text for host; individual configs *should* override, but this may
catch some flubs
_define__(<_HOST__>,<machine specific>)

"Machine Dependent" nodename
_define__(<_MACH_DEP__>,<Machine Dependent>)

_divert__<>
