# Info from Johann Klasek <jk@auto.tuwien.ac.at>
# Merged by Andy Dougherty  <doughera@lafcol.lafayette.edu>
# Last revised	Tue Mar 16 19:12:22 EET 1999 by
# Jarkko Hietaniemi <jhi@iki.fi>

# uname -a looks like
# DomainOS newton 10.4.1 bsd4.3 425t

# We want to use both BSD includes and some of the features from the
# /sys5 includes.
ccflags="$ccflags -A cpu,mathchip -I`pwd`/apollo -I/usr/include -I/sys5/usr/include"

# When Apollo runs a script with "#!", it sets argv[0] to the script name.
toke_cflags='ccflags="$ccflags -DARG_ZERO_IS_SCRIPT"'

# These adjustments are necessary (why?) to compile malloc.c.
freetype='void'
i_malloc='undef'
malloctype='void *'

# This info is left over from perl4.  
cat <<'EOF' >&4
Some tests may fail unless you use 'chacl -B'.  Also, op/stat
test 2 may fail occasionally because Apollo doesn't guarantee
that mtime will be equal to ctime on a newly created unmodified
file.  Finally, the sleep test will sometimes fail.  See the
sleep(3) man page to learn why.

See hints/apollo.sh for hints on running h2ph.

And a note on ccflags:

    Lastly, while -A cpu,mathchip generates optimal code for your DN3500
    running sr10.3, be aware that you should be using -A cpu,mathlib_sr10
    if your perl must also run on any machines running sr10.0, sr10.1, or
    sr10.2.  The -A cpu,mathchip option generates code that doesn't work on
    pre-sr10.3 nodes.  See the cc(1) man page for more details.
						-- Steve Vinoski

EOF

# Running h2ph, on the other hand, presents a challenge. 

#The perl header files have to be generated with following commands

#sed 's|/usr/include|/sys5/usr/include|g' h2ph >h2ph.new && chmod +x h2ph.new
#(set cdir=`pwd`; cd /sys5/usr/include; $cdir/h2ph.new sys/* )
#(set cdir=`pwd`; cd /usr/include; $cdir/h2ph * sys/* machine/*)

#The SYS5 headers (only sys) are overlayed by the BSD headers.  It  seems
#all ok, but once I am going into details,  a  lot  of  limitations  from
#'h2ph' are coming up. Lines like "#define NODEV (dev_t)(-1)"  result  in
#syntax errors as converted by h2ph. 

# Generally, h2ph might need a lot of help.
