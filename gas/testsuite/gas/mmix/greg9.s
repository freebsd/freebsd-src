% Check that we can allocate max number of GREGs.
% A bit cheesy: we allocate anonymous GREGs with no handle.  This isn't
% generally useful, but it helps keeping the number of lines down, and we
% check that the right thing happened in the object file.
Main SWYM 0
	.rept 222
	GREG
	.endr
