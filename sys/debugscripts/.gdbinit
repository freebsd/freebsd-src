# $FreeBSD#
# .gdbinit file for remote serial debugging.
# see gdbinit(9) for further details.
#
# The following lines (down to "end" comment) may need to be changed
file kernel.debug
set output-radix 16
set height 70
set width 120

# Connect to remote target
define tr
set remotebaud 9600
# Remote debugging port
target remote /dev/cuaa0
end

# Get symbols from klds.  This is a little fiddly, but very fast.
define getsyms
kldstat
echo Select the list above with the mouse, paste into the screen\n
echo and then press ^D.  Yes, this is annoying.\n
# This should be the path of the real modules directory.
shell asf modules/src/FreeBSD/5-CURRENT-ZAPHOD/src/sys/modules 
source .asf
end

# End of things you're likely to need to change.

set remotetimeout 1
set complaints 1
set print pretty
dir ../../..
document tr
Attach to a remote kernel via serial port
end

source gdbinit.kernel
source gdbinit.vinum

# Attach to the remote kernel
tr
# And get the symbols from klds
getsyms
