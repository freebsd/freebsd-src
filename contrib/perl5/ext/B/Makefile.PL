use ExtUtils::MakeMaker;
use Config;
use File::Spec;

my $e = $Config{'exe_ext'};
my $o = $Config{'obj_ext'};
my $exeout_flag = '-o ';
if ($^O eq 'MSWin32') {
    if ($Config{'cc'} =~ /^cl/i) {
	$exeout_flag = '-Fe';
    }
    elsif ($Config{'cc'} =~ /^bcc/i) {
	$exeout_flag = '-e';
    }
}

WriteMakefile(
    NAME	=> "B",
    VERSION	=> "a5",
    PL_FILES	=> { 'defsubs_h.PL' => 'defsubs.h' },
    MAN3PODS	=> {},
    clean	=> {
	FILES		=> "perl$e *$o B.c defsubs.h *~"
    }
);   

package MY;

sub post_constants {
    "\nLIBS = $Config::Config{libs}\n"
}    

sub upupfile {
    File::Spec->catfile(File::Spec->updir,
			File::Spec->updir, $_[0]);
}

sub MY::postamble {
    my $op_h   = upupfile('op.h');
    my $cop_h  = upupfile('cop.h');
    my $noecho = shift->{NOECHO};
"
B\$(OBJ_EXT) : defsubs.h              

defsubs.h :: $op_h $cop_h                     
	$noecho \$(NOOP)
" 
}
