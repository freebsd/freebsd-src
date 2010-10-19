#source: break.s
#as: --underscore --em=criself --march=common_v10_v32
#objdump: -p

# Check that different command-line options result in different
# machine-type stamps on the object files.

.*:     file format elf32-us-cris
private flags = 5: \[symbols have a _ prefix\] \[v10 and v32\]
