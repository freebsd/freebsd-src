# notarget: cris*-*-linux-gnu
# source: start1.s --march=v0_v10
# source: move-1.s --march=common_v10_v32
# as: --em=criself
# ld: -m criself
# objdump: -p

# Test that linking a v10+v32 compatible object to a v10 object
# does work and results in the output marked as a v10 object.
# Source code and "-m criself" doesn't work with *-linux-gnu.

#...
private flags = 1: \[symbols have a _ prefix\]
#pass
