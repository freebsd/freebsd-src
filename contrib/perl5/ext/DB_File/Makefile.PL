use ExtUtils::MakeMaker 5.16 ;
use Config ;

# OS2 is a special case, so check for it now.
my $OS2 = "-DOS2" if $Config{'osname'} eq 'os2' ;

my $LIB = "-ldb" ;
# so is win32
$LIB = "-llibdb" if $^O eq 'MSWin32' ;

WriteMakefile(
	NAME 		=> 'DB_File',
	LIBS 		=> ["-L/usr/local/lib $LIB"],
        MAN3PODS        => {},         # Pods will be built by installman.
	#INC		=> '-I/usr/local/include',
	VERSION_FROM	=> 'DB_File.pm',
	OBJECT		=> 'version$(OBJ_EXT) DB_File$(OBJ_EXT)',
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> $OS2 || "",
	INC => ($^O eq "MacOS" ? "-i ::::db:include" : "")
	);

sub MY::postamble {
      '
version$(OBJ_EXT):	version.c

' ;
}

