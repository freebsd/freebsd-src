/* This file isn't directly used by the test suite; it uses
   elf_e_flags.s.  However, I figured it would be nice to provide the
   source code from which the .s file was generated.

   It was compiled as follows:

   mips64-elf-gcc -m4650 -S -O elf_e_flags.c

   We use the -m4650 flag to get the 4650-specific 'mul' instruction
   in there; the test suite wants to be sure that GAS's -m4650 flag
   will indeed cause it to generate the 4650 mul instruction, and not
   expand it as a macro.

   Ian 10 June 1999: I tweaked the resulting assembler file so that it
   would generate the same code when gas was configured for mips-elf
   and for mips64-elf.

   18 October 2000: Chris Demetriou <cgd@sibyte.com> tweaked the code so
   that it would always generate enough zero-padding at the end to make
   objdump print "...", so that the test would be successful even on
   machines that pad results to cache line or other boundaries
   (e.g. mips-linux). */

int
foo (int a, int b)
{
  return (a * b) + 1;
}

int
main ()
{
  return 0;
}
