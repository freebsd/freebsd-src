# If this doesn't work, try specifying 'none' for hints.
d_castneg=undef
libswanted=`echo $libswanted | sed -e 's/socket /socket seq /'`

# Reported by Craig Milo Rogers <Rogers@ISI.EDU>
# Date: Tue, 30 Jan 96 15:29:26 PST
d_casti32=undef
