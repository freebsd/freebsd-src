use ExtUtils::MakeMaker;
use Config;

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

sub postamble {
'
B$(OBJ_EXT) : defsubs.h 
'
}
