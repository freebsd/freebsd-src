#source: abs32-1.s
#as: --underscore --em=criself --march=v32
#objdump: -p

# Check that different command-line options result in different
# machine-type stamps on the object files.

.*:     file format elf32-us-cris
private flags = 3: \[symbols have a _ prefix\] \[v32\]
