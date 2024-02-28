# Unmodified nawk prints the 16 bit exit status divided by 256, but
# does so using floating point arithmetic, yielding strange results.
#
# The fix is to use the various macros defined for wait(2) and to
# use the signal number + 256 for death by signal, or signal number + 512
# for death by signal with core dump.

BEGIN {
	status = system("exit 42")
	print "normal status", status

	status = system("kill -HUP $$")
	print "death by signal status", status

	status = system("kill -ABRT $$")
	print "death by signal with core dump status", status

	system("rm -f core*")
}
