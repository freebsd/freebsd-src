use ExtUtils::MakeMaker;

@VMS = ($^O eq 'VMS') ? (MAN3PODS => {}) : ();

WriteMakefile(
    NAME	 => 'Errno',
    VERSION_FROM => 'Errno_pm.PL',
    MAN3PODS     => {}, 	# Pods will be built by installman.
    PL_FILES	 => {'Errno_pm.PL'=>'Errno.pm'},
    PM		 => {'Errno.pm' => '$(INST_LIBDIR)/Errno.pm'},
    'clean'	 => {FILES => 'Errno.pm'},
    'dist'	 => {
	COMPRESS => 'gzip -9f',
	SUFFIX   => '.gz', 
	DIST_DEFAULT => 'd/Errno.pm tardist',
    },
    @VMS,
);

sub MY::postamble {
    my $TARG = MM->catfile('d','Errno.pm');
qq!$TARG : Makefile
	echo '#This is a dummy file so CPAN will find a VERSION' > $TARG
	echo 'package Errno;' >> $TARG
	echo '\$\$VERSION = "\$(VERSION)";' >>$TARG
	echo '#This is to make sure require will return an error' >>$TARG
	echo '0;' >>$TARG

!
}
