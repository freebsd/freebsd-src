# $Id: Embed.pm,v 1.2501 $
require 5.002;

package ExtUtils::Embed;
require Exporter;
require FileHandle;
use Config;
use Getopt::Std;

#Only when we need them
#require ExtUtils::MakeMaker;
#require ExtUtils::Liblist;

use vars qw(@ISA @EXPORT $VERSION
	    @Extensions $Verbose $lib_ext
	    $opt_o $opt_s 
	    );
use strict;

$VERSION = sprintf("%d.%02d", q$Revision: 1.2505 $ =~ /(\d+)\.(\d+)/);

@ISA = qw(Exporter);
@EXPORT = qw(&xsinit &ldopts 
	     &ccopts &ccflags &ccdlflags &perl_inc
	     &xsi_header &xsi_protos &xsi_body);

#let's have Miniperl borrow from us instead
#require ExtUtils::Miniperl;
#*canon = \&ExtUtils::Miniperl::canon;

$Verbose = 0;
$lib_ext = $Config{lib_ext} || '.a';

sub is_cmd { $0 eq '-e' }

sub my_return {
    my $val = shift;
    if(is_cmd) {
	print $val;
    }
    else {
	return $val;
    }
}

sub is_perl_object {
    $Config{ccflags} =~ /-DPERL_OBJECT/;  
}

sub xsinit { 
    my($file, $std, $mods) = @_;
    my($fh,@mods,%seen);
    $file ||= "perlxsi.c";
    my $xsinit_proto = is_perl_object() ? "CPERLarg" : "void";

    if (@_) {
       @mods = @$mods if $mods;
    }
    else {
       getopts('o:s:');
       $file = $opt_o if defined $opt_o;
       $std  = $opt_s  if defined $opt_s;
       @mods = @ARGV;
    }
    $std = 1 unless scalar @mods;

    if ($file eq "STDOUT") {
	$fh = \*STDOUT;
    }
    else {
	$fh = new FileHandle "> $file";
    }

    push(@mods, static_ext()) if defined $std;
    @mods = grep(!$seen{$_}++, @mods);

    print $fh &xsi_header();
    print $fh "EXTERN_C void xs_init _(($xsinit_proto));\n\n";     
    print $fh &xsi_protos(@mods);

    print $fh "\nEXTERN_C void\nxs_init($xsinit_proto)\n{\n";
    print $fh &xsi_body(@mods);
    print $fh "}\n";

}

sub xsi_header {
    return <<EOF;
#if defined(__cplusplus) && !defined(PERL_OBJECT)
#define is_cplusplus
#endif

#ifdef is_cplusplus
extern "C" {
#endif

#include <EXTERN.h>
#include <perl.h>
#ifdef PERL_OBJECT
#define NO_XSLOCKS
#include <XSUB.h>
#include "win32iop.h"
#include <fcntl.h>
#include <perlhost.h>
#endif
#ifdef is_cplusplus
}
#  ifndef EXTERN_C
#    define EXTERN_C extern "C"
#  endif
#else
#  ifndef EXTERN_C
#    define EXTERN_C extern
#  endif
#endif

EOF
}    

sub xsi_protos {
    my(@exts) = @_;
    my(@retval,%seen);
    my $boot_proto = is_perl_object() ? 
	"CV* cv _CPERLarg" : "CV* cv";
    foreach $_ (@exts){
        my($pname) = canon('/', $_);
        my($mname, $cname);
        ($mname = $pname) =~ s!/!::!g;
        ($cname = $pname) =~ s!/!__!g;
	my($ccode) = "EXTERN_C void boot_${cname} _(($boot_proto));\n";
	next if $seen{$ccode}++;
        push(@retval, $ccode);
    }
    return join '', @retval;
}

sub xsi_body {
    my(@exts) = @_;
    my($pname,@retval,%seen);
    my($dl) = canon('/','DynaLoader');
    push(@retval, "\tchar *file = __FILE__;\n");
    push(@retval, "\tdXSUB_SYS;\n") if $] > 5.002;
    push(@retval, "\n");

    foreach $_ (@exts){
        my($pname) = canon('/', $_);
        my($mname, $cname, $ccode);
        ($mname = $pname) =~ s!/!::!g;
        ($cname = $pname) =~ s!/!__!g;
        if ($pname eq $dl){
            # Must NOT install 'DynaLoader::boot_DynaLoader' as 'bootstrap'!
            # boot_DynaLoader is called directly in DynaLoader.pm
            $ccode = "\t/* DynaLoader is a special case */\n\tnewXS(\"${mname}::boot_${cname}\", boot_${cname}, file);\n";
            push(@retval, $ccode) unless $seen{$ccode}++;
        } else {
            $ccode = "\tnewXS(\"${mname}::bootstrap\", boot_${cname}, file);\n";
            push(@retval, $ccode) unless $seen{$ccode}++;
        }
    }
    return join '', @retval;
}

sub static_ext {
    unless (scalar @Extensions) {
	@Extensions = sort split /\s+/, $Config{static_ext};
	unshift @Extensions, qw(DynaLoader);
    }
    @Extensions;
}

sub ldopts {
    require ExtUtils::MakeMaker;
    require ExtUtils::Liblist;
    my($std,$mods,$link_args,$path) = @_;
    my(@mods,@link_args,@argv);
    my($dllib,$config_libs,@potential_libs,@path);
    local($") = ' ' unless $" eq ' ';
    my $MM = bless {} => 'MY';
    if (scalar @_) {
       @link_args = @$link_args if $link_args;
       @mods = @$mods if $mods;
    }
    else {
       @argv = @ARGV;
       #hmm
       while($_ = shift @argv) {
	   /^-std$/  && do { $std = 1; next; };
	   /^--$/    && do { @link_args = @argv; last; };
	   /^-I(.*)/ && do { $path = $1 || shift @argv; next; };
	   push(@mods, $_); 
       }
    }
    $std = 1 unless scalar @link_args;
    @path = $path ? split(/:/, $path) : @INC;

    push(@potential_libs, @link_args)    if scalar @link_args;
    push(@potential_libs, $Config{libs}) if defined $std;

    push(@mods, static_ext()) if $std;

    my($mod,@ns,$root,$sub,$extra,$archive,@archives);
    print STDERR "Searching (@path) for archives\n" if $Verbose;
    foreach $mod (@mods) {
	@ns = split(/::|\/|\\/, $mod);
	$sub = $ns[-1];
	$root = $MM->catdir(@ns);
	
	print STDERR "searching for '$sub${lib_ext}'\n" if $Verbose;
	foreach (@path) {
	    next unless -e ($archive = $MM->catdir($_,"auto",$root,"$sub$lib_ext"));
	    push @archives, $archive;
	    if(-e ($extra = $MM->catdir($_,"auto",$root,"extralibs.ld"))) {
		local(*FH); 
		if(open(FH, $extra)) {
		    my($libs) = <FH>; chomp $libs;
		    push @potential_libs, split /\s+/, $libs;
		}
		else {  
		    warn "Couldn't open '$extra'"; 
		}
	    }
	    last;
	}
    }
    #print STDERR "\@potential_libs = @potential_libs\n";

    my $libperl = (grep(/^-l\w*perl\w*$/, @link_args))[0] || "-lperl";

    my($extralibs, $bsloadlibs, $ldloadlibs, $ld_run_path) =
	$MM->ext(join ' ', 
		 $MM->catdir("-L$Config{archlibexp}", "CORE"), " $libperl", 
		 @potential_libs);

    my $ld_or_bs = $bsloadlibs || $ldloadlibs;
    print STDERR "bs: $bsloadlibs ** ld: $ldloadlibs" if $Verbose;
    my $linkage = "$Config{ccdlflags} $Config{ldflags} @archives $ld_or_bs";
    print STDERR "ldopts: '$linkage'\n" if $Verbose;

    return $linkage if scalar @_;
    my_return("$linkage\n");
}

sub ccflags {
    my_return(" $Config{ccflags} ");
}

sub ccdlflags {
    my_return(" $Config{ccdlflags} ");
}

sub perl_inc {
    my_return(" -I$Config{archlibexp}/CORE ");
}

sub ccopts {
   ccflags . perl_inc;
}

sub canon {
    my($as, @ext) = @_;
    foreach(@ext) {
       # might be X::Y or lib/auto/X/Y/Y.a
       next if s!::!/!g;
       s:^(lib|ext)/(auto/)?::;
       s:/\w+\.\w+$::;
    }
    grep(s:/:$as:, @ext) if ($as ne '/');
    @ext;
}

__END__

=head1 NAME

ExtUtils::Embed - Utilities for embedding Perl in C/C++ applications

=head1 SYNOPSIS


 perl -MExtUtils::Embed -e xsinit 
 perl -MExtUtils::Embed -e ldopts 

=head1 DESCRIPTION

ExtUtils::Embed provides utility functions for embedding a Perl interpreter
and extensions in your C/C++ applications.  
Typically, an application B<Makefile> will invoke ExtUtils::Embed
functions while building your application.  

=head1 @EXPORT

ExtUtils::Embed exports the following functions:

xsinit(), ldopts(), ccopts(), perl_inc(), ccflags(), 
ccdlflags(), xsi_header(), xsi_protos(), xsi_body()

=head1 FUNCTIONS

=over

=item xsinit()

Generate C/C++ code for the XS initializer function.

When invoked as C<`perl -MExtUtils::Embed -e xsinit --`>
the following options are recognized:

B<-o> E<lt>output filenameE<gt> (Defaults to B<perlxsi.c>)

B<-o STDOUT> will print to STDOUT.

B<-std> (Write code for extensions that are linked with the current Perl.)

Any additional arguments are expected to be names of modules
to generate code for.

When invoked with parameters the following are accepted and optional:

C<xsinit($filename,$std,[@modules])>

Where,

B<$filename> is equivalent to the B<-o> option.

B<$std> is boolean, equivalent to the B<-std> option.  

B<[@modules]> is an array ref, same as additional arguments mentioned above.

=item Examples


 perl -MExtUtils::Embed -e xsinit -- -o xsinit.c Socket


This will generate code with an B<xs_init> function that glues the perl B<Socket::bootstrap> function 
to the C B<boot_Socket> function and writes it to a file named "xsinit.c".

Note that B<DynaLoader> is a special case where it must call B<boot_DynaLoader> directly.

 perl -MExtUtils::Embed -e xsinit


This will generate code for linking with B<DynaLoader> and 
each static extension found in B<$Config{static_ext}>.
The code is written to the default file name B<perlxsi.c>.


 perl -MExtUtils::Embed -e xsinit -- -o xsinit.c -std DBI DBD::Oracle


Here, code is written for all the currently linked extensions along with code
for B<DBI> and B<DBD::Oracle>.

If you have a working B<DynaLoader> then there is rarely any need to statically link in any 
other extensions.

=item ldopts()

Output arguments for linking the Perl library and extensions to your
application.

When invoked as C<`perl -MExtUtils::Embed -e ldopts --`>
the following options are recognized:

B<-std> 

Output arguments for linking the Perl library and any extensions linked
with the current Perl.

B<-I> E<lt>path1:path2E<gt>

Search path for ModuleName.a archives.  
Default path is B<@INC>.
Library archives are expected to be found as 
B</some/path/auto/ModuleName/ModuleName.a>
For example, when looking for B<Socket.a> relative to a search path, 
we should find B<auto/Socket/Socket.a>  

When looking for B<DBD::Oracle> relative to a search path,
we should find B<auto/DBD/Oracle/Oracle.a>

Keep in mind, you can always supply B</my/own/path/ModuleName.a>
as an additional linker argument.

B<-->  E<lt>list of linker argsE<gt>

Additional linker arguments to be considered.

Any additional arguments found before the B<--> token 
are expected to be names of modules to generate code for.

When invoked with parameters the following are accepted and optional:

C<ldopts($std,[@modules],[@link_args],$path)>

Where,

B<$std> is boolean, equivalent to the B<-std> option.  

B<[@modules]> is equivalent to additional arguments found before the B<--> token.

B<[@link_args]> is equivalent to arguments found after the B<--> token.

B<$path> is equivalent to the B<-I> option.

In addition, when ldopts is called with parameters, it will return the argument string
rather than print it to STDOUT.

=item Examples


 perl -MExtUtils::Embed -e ldopts


This will print arguments for linking with B<libperl.a>, B<DynaLoader> and 
extensions found in B<$Config{static_ext}>.  This includes libraries
found in B<$Config{libs}> and the first ModuleName.a library
for each extension that is found by searching B<@INC> or the path 
specified by the B<-I> option.  
In addition, when ModuleName.a is found, additional linker arguments
are picked up from the B<extralibs.ld> file in the same directory.


 perl -MExtUtils::Embed -e ldopts -- -std Socket


This will do the same as the above example, along with printing additional arguments for linking with the B<Socket> extension.


 perl -MExtUtils::Embed -e ldopts -- DynaLoader


This will print arguments for linking with just the B<DynaLoader> extension
and B<libperl.a>.


 perl -MExtUtils::Embed -e ldopts -- -std Msql -- -L/usr/msql/lib -lmsql


Any arguments after the second '--' token are additional linker
arguments that will be examined for potential conflict.  If there is no
conflict, the additional arguments will be part of the output.  


=item perl_inc()

For including perl header files this function simply prints:

 -I$Config{archlibexp}/CORE  

So, rather than having to say:

 perl -MConfig -e 'print "-I$Config{archlibexp}/CORE"'

Just say:

 perl -MExtUtils::Embed -e perl_inc

=item ccflags(), ccdlflags()

These functions simply print $Config{ccflags} and $Config{ccdlflags}

=item ccopts()

This function combines perl_inc(), ccflags() and ccdlflags() into one.

=item xsi_header()

This function simply returns a string defining the same B<EXTERN_C> macro as
B<perlmain.c> along with #including B<perl.h> and B<EXTERN.h>.  

=item xsi_protos(@modules)

This function returns a string of B<boot_$ModuleName> prototypes for each @modules.

=item xsi_body(@modules)

This function returns a string of calls to B<newXS()> that glue the module B<bootstrap>
function to B<boot_ModuleName> for each @modules.

B<xsinit()> uses the xsi_* functions to generate most of it's code.

=back

=head1 EXAMPLES

For examples on how to use B<ExtUtils::Embed> for building C/C++ applications
with embedded perl, see the eg/ directory and L<perlembed>.

=head1 SEE ALSO

L<perlembed>

=head1 AUTHOR

Doug MacEachern E<lt>F<dougm@osf.org>E<gt>

Based on ideas from Tim Bunce E<lt>F<Tim.Bunce@ig.co.uk>E<gt> and
B<minimod.pl> by Andreas Koenig E<lt>F<k@anna.in-berlin.de>E<gt> and Tim Bunce.

=cut

