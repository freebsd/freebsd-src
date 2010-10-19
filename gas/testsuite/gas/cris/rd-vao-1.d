#source: abs32-1.s
#as: --underscore --em=criself --march=v0_v10
#objdump: -p

# Check that different command-line options result in different
# machine-type stamps on the object files.  The source file
# isn't important, as long the code assembles for the machine we
# specify.

.*:     file format elf32-us-cris
private flags = 1: \[symbols have a _ prefix\]
