use ExtUtils::MakeMaker;

# The existence of the ./sdbm/Makefile.PL file causes MakeMaker
# to automatically include Makefile code for the targets
#	config, all, clean, realclean and sdbm/Makefile
# which perform the corresponding actions in the subdirectory.

$define = ($^O eq 'MSWin32') ? '-DMSDOS' : '';
if ($^O eq 'MSWin32') { $myextlib = 'sdbm\\libsdbm$(LIB_EXT)'; }
else                  { $myextlib = 'sdbm/libsdbm$(LIB_EXT)';  }

WriteMakefile(
              NAME	=> 'SDBM_File',
              MYEXTLIB => $myextlib,
              MAN3PODS 	=> {}, 	# Pods will be built by installman.
              XSPROTOARG => '-noprototypes', 		# XXX remove later?
              VERSION_FROM => 'SDBM_File.pm',
              DEFINE => $define,
             );

sub MY::postamble {
  if ($^O ne 'VMS') {
    '
$(MYEXTLIB): sdbm/Makefile
	cd sdbm && $(MAKE) all
';
  } else {
  '   
$(MYEXTLIB) : [.sdbm]descrip.mms
	set def [.sdbm]
    $(MMS) all
    set def [-]
';  
  }
}
