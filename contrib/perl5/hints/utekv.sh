# XD88/10 UTekV hints by Kaveh Ghazi (ghazi@caip.rutgers.edu)  2/11/92
# Modified by Andy Dougherty <doughera@lafcol.lafayette.edu>   4 Oct. 1994

# The -X18 is only if you are using the Greenhills compiler.
ccflags="$ccflags -X18"

usemymalloc='y'

echo " "								    >&4
echo "NOTE: You may have to take out makefile dependencies on the files in" >&4
echo "/usr/include (i.e. /usr/include/ctype.h) or the make will fail.  A"   >&4
echo "simple 'grep -v /usr/include/ makefile' should suffice."		    >&4
