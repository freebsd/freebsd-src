# notarget: cris*-*-linux-gnu
# source: start1.s --march=v32
# source: move-1.s --march=v0_v10
# as: --em=criself
# ld: -m criself
# error: contains non-CRIS-v32 code

# Test that linking a (classic) v10 object to a v32 object does
# not work.  Source code and "-m criself" doesn't work with *-linux-gnu.

