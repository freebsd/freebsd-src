# $FreeBSD$

# Anything that you add to OPTS will be added to the build rules for each
# program built.

OPTS= -DNOPAM -DRELEASE_CRUNCH -DNOSECURE -DNOCRYPT -DNONETGRAPH -DNOIPSEC


# Per program options can be added also...
#ppp_OPTS= -DNOKLDLOAD -DNOINET6 -DNONAT -DNOATM -DNOSUID -DHAVE_DES -DNORADIUS -DNOI4B -DNONETGRAPH

#end
