#
# Create the export list for perl.
#
# Needed by WIN32 and OS/2 for creating perl.dll
# and by AIX for creating libperl.a when -Dusershrplib is in effect.
#
# reads global.sym, pp.sym, perlvars.h, intrpvar.h, thrdvar.h, config.h
# On OS/2 reads miniperl.map as well

my $PLATFORM;
my $CCTYPE;

my %bincompat5005 =
      (
       Perl_call_atexit		=>	"perl_atexit",
       Perl_eval_sv		=>	"perl_eval_sv",
       Perl_eval_pv		=>	"perl_eval_pv",
       Perl_call_argv		=>	"perl_call_argv",
       Perl_call_method		=>	"perl_call_method",
       Perl_call_pv		=>	"perl_call_pv",
       Perl_call_sv		=>	"perl_call_sv",
       Perl_get_av		=>	"perl_get_av",
       Perl_get_cv		=>	"perl_get_cv",
       Perl_get_hv		=>	"perl_get_hv",
       Perl_get_sv		=>	"perl_get_sv",
       Perl_init_i18nl10n	=>	"perl_init_i18nl10n",
       Perl_init_i18nl14n	=>	"perl_init_i18nl14n",
       Perl_new_collate		=>	"perl_new_collate",
       Perl_new_ctype		=>	"perl_new_ctype",
       Perl_new_numeric		=>	"perl_new_numeric",
       Perl_require_pv		=>	"perl_require_pv",
       Perl_safesyscalloc	=>	"Perl_safecalloc",
       Perl_safesysfree		=>	"Perl_safefree",
       Perl_safesysmalloc	=>	"Perl_safemalloc",
       Perl_safesysrealloc	=>	"Perl_saferealloc",
       Perl_set_numeric_local	=>	"perl_set_numeric_local",
       Perl_set_numeric_standard  =>	"perl_set_numeric_standard",
       Perl_malloc		=>	"malloc",
       Perl_mfree		=>	"free",
       Perl_realloc		=>	"realloc",
       Perl_calloc		=>	"calloc",
      );

my $bincompat5005 = join("|", keys %bincompat5005);

while (@ARGV) {
    my $flag = shift;
    $define{$1} = 1 if ($flag =~ /^-D(\w+)$/);
    $define{$1} = $2 if ($flag =~ /^-D(\w+)=(.+)$/);
    $CCTYPE   = $1 if ($flag =~ /^CCTYPE=(\w+)$/);
    $PLATFORM = $1 if ($flag =~ /^PLATFORM=(\w+)$/);
}

my @PLATFORM = qw(aix win32 os2 MacOS);
my %PLATFORM;
@PLATFORM{@PLATFORM} = ();

defined $PLATFORM || die "PLATFORM undefined, must be one of: @PLATFORM\n";
exists $PLATFORM{$PLATFORM} || die "PLATFORM must be one of: @PLATFORM\n"; 

my $config_sh   = "config.sh";
my $config_h    = "config.h";
my $thrdvar_h   = "thrdvar.h";
my $intrpvar_h  = "intrpvar.h";
my $perlvars_h  = "perlvars.h";
my $global_sym  = "global.sym";
my $pp_sym      = "pp.sym";
my $globvar_sym = "globvar.sym";
my $perlio_sym  = "perlio.sym";

if ($PLATFORM eq 'aix') { 
    # Nothing for now.
}
elsif ($PLATFORM eq 'win32') {
    $CCTYPE = "MSVC" unless defined $CCTYPE;
    foreach ($thrdvar_h, $intrpvar_h, $perlvars_h, $global_sym, $pp_sym, $globvar_sym) {
	s!^!..\\!;
    }
}
elsif ($PLATFORM eq 'MacOS') {
    foreach ($thrdvar_h, $intrpvar_h, $perlvars_h, $global_sym,
		$pp_sym, $globvar_sym, $perlio_sym) {
	s!^!::!;
    }
}

unless ($PLATFORM eq 'win32' || $PLATFORM eq 'MacOS') {
    open(CFG,$config_sh) || die "Cannot open $config_sh: $!\n";
    while (<CFG>) {
	if (/^(?:ccflags|optimize)='(.+)'$/) {
	    $_ = $1;
	    $define{$1} = 1 while /-D(\w+)/g;
	}
	if ($PLATFORM eq 'os2') {
	    $CONFIG_ARGS = $1 if /^(?:config_args)='(.+)'$/;
	    $ARCHNAME = $1 if /^(?:archname)='(.+)'$/;
	}
    }
    close(CFG);
}

open(CFG,$config_h) || die "Cannot open $config_h: $!\n";
while (<CFG>) {
    $define{$1} = 1 if /^\s*#\s*define\s+(MYMALLOC)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(USE_5005THREADS)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(USE_ITHREADS)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(USE_PERLIO)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(USE_SFIO)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(MULTIPLICITY)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(PERL_IMPLICIT_SYS)\b/;
    $define{$1} = 1 if /^\s*#\s*define\s+(PERL_BINCOMPAT_5005)\b/;
}
close(CFG);

# perl.h logic duplication begins

if ($define{USE_ITHREADS}) {
    if (!$define{MULTIPLICITY} && !$define{PERL_OBJECT}) {
        $define{MULTIPLICITY} = 1;
    }
}

$define{PERL_IMPLICIT_CONTEXT} ||=
    $define{USE_ITHREADS} ||
    $define{USE_5005THREADS}  ||
    $define{MULTIPLICITY} ;

if ($define{PERL_CAPI}) {
    delete $define{PERL_OBJECT};
    $define{MULTIPLICITY} = 1; 
    $define{PERL_IMPLICIT_CONTEXT} = 1;
    $define{PERL_IMPLICIT_SYS}     = 1;
}

if ($define{PERL_OBJECT}) {
    $define{PERL_IMPLICIT_CONTEXT} = 1;
    $define{PERL_IMPLICIT_SYS}     = 1;
}

# perl.h logic duplication ends

if ($PLATFORM eq 'win32') {
    warn join(' ',keys %define)."\n";
    print "LIBRARY Perl56\n";
    print "DESCRIPTION 'Perl interpreter'\n";
    print "EXPORTS\n";
    if ($define{PERL_IMPLICIT_SYS}) {
	output_symbol("perl_get_host_info");
	output_symbol("perl_alloc_override");
    }
}
elsif ($PLATFORM eq 'os2') {
    ($v = $]) =~ s/(\d\.\d\d\d)(\d\d)$/$1_$2/;
    $v .= '-thread' if $ARCHNAME =~ /-thread/;
    #$sum = 0;
    #for (split //, $v) {
    #	$sum = ($sum * 33) + ord;
    #	$sum &= 0xffffff;
    #}
    #$sum += $sum >> 5;
    #$sum &= 0xffff;
    #$sum = printf '%X', $sum;
    ($dll = $define{PERL_DLL}) =~ s/\.dll$//i;
    # print STDERR "'$dll' <= '$define{PERL_DLL}'\n";
    print <<"---EOP---";
LIBRARY '$dll' INITINSTANCE TERMINSTANCE
DESCRIPTION '\@#perl5-porters\@perl.org:$v#\@ Perl interpreter'
STACKSIZE 32768
CODE LOADONCALL
DATA LOADONCALL NONSHARED MULTIPLE
EXPORTS
---EOP---
}
elsif ($PLATFORM eq 'aix') {
    print "#!\n";
}

my %skip;
my %export;

sub skip_symbols {
    my $list = shift;
    foreach my $symbol (@$list) {
	$skip{$symbol} = 1;
    }
}

sub emit_symbols {
    my $list = shift;
    foreach my $symbol (@$list) {
	my $skipsym = $symbol;
	# XXX hack
	if ($define{PERL_OBJECT} || $define{MULTIPLICITY}) {
	    $skipsym =~ s/^Perl_[GIT](\w+)_ptr$/PL_$1/;
	}
	emit_symbol($symbol) unless exists $skip{$skipsym};
    }
}

if ($PLATFORM eq 'win32') {
    skip_symbols [qw(
		     PL_statusvalue_vms
		     PL_archpat_auto
		     PL_cryptseen
		     PL_DBcv
		     PL_generation
		     PL_lastgotoprobe
		     PL_linestart
		     PL_modcount
		     PL_pending_ident
		     PL_sortcxix
		     PL_sublex_info
		     PL_timesbuf
		     main
		     Perl_ErrorNo
		     Perl_GetVars
		     Perl_do_exec3
		     Perl_do_ipcctl
		     Perl_do_ipcget
		     Perl_do_msgrcv
		     Perl_do_msgsnd
		     Perl_do_semop
		     Perl_do_shmio
		     Perl_dump_fds
		     Perl_init_thread_intern
		     Perl_my_bzero
		     Perl_my_htonl
		     Perl_my_ntohl
		     Perl_my_swap
		     Perl_my_chsize
		     Perl_same_dirent
		     Perl_setenv_getix
		     Perl_unlnk
		     Perl_watch
		     Perl_safexcalloc
		     Perl_safexmalloc
		     Perl_safexfree
		     Perl_safexrealloc
		     Perl_my_memcmp
		     Perl_my_memset
		     PL_cshlen
		     PL_cshname
		     PL_opsave
		     Perl_do_exec
		     Perl_getenv_len
		     Perl_my_pclose
		     Perl_my_popen
		     )];
}
elsif ($PLATFORM eq 'aix') {
    skip_symbols([qw(
		     Perl_dump_fds
		     Perl_ErrorNo
		     Perl_GetVars
		     Perl_my_bcopy
		     Perl_my_bzero
		     Perl_my_chsize
		     Perl_my_htonl
		     Perl_my_memcmp
		     Perl_my_memset
		     Perl_my_ntohl
		     Perl_my_swap
		     Perl_safexcalloc
		     Perl_safexfree
		     Perl_safexmalloc
		     Perl_safexrealloc
		     Perl_same_dirent
		     Perl_unlnk
		     Perl_sys_intern_clear
		     Perl_sys_intern_dup
		     Perl_sys_intern_init
		     PL_cryptseen
		     PL_opsave
		     PL_statusvalue_vms
		     PL_sys_intern
		     )]);
}
elsif ($PLATFORM eq 'os2') {
    emit_symbols([qw(
		    ctermid
		    get_sysinfo
		    Perl_OS2_init
		    OS2_Perl_data
		    dlopen
		    dlsym
		    dlerror
		    dlclose
		    my_tmpfile
		    my_tmpnam
		    my_flock
		    my_rmdir
		    my_mkdir
		    malloc_mutex
		    threads_mutex
		    nthreads
		    nthreads_cond
		    os2_cond_wait
		    os2_stat
		    pthread_join
		    pthread_create
		    pthread_detach
		    XS_Cwd_change_drive
		    XS_Cwd_current_drive
		    XS_Cwd_extLibpath
		    XS_Cwd_extLibpath_set
		    XS_Cwd_sys_abspath
		    XS_Cwd_sys_chdir
		    XS_Cwd_sys_cwd
		    XS_Cwd_sys_is_absolute
		    XS_Cwd_sys_is_relative
		    XS_Cwd_sys_is_rooted
		    XS_DynaLoader_mod2fname
		    XS_File__Copy_syscopy
		    Perl_Register_MQ
		    Perl_Deregister_MQ
		    Perl_Serve_Messages
		    Perl_Process_Messages
		    init_PMWIN_entries
		    PMWIN_entries
		    Perl_hab_GET
		    )]);
}
elsif ($PLATFORM eq 'MacOS') {
    skip_symbols [qw(
		    Perl_GetVars
		    PL_cryptseen
		    PL_cshlen
		    PL_cshname
		    PL_statusvalue_vms
		    PL_sys_intern
		    PL_opsave
		    PL_timesbuf
		    Perl_dump_fds
		    Perl_my_bcopy
		    Perl_my_bzero
		    Perl_my_chsize
		    Perl_my_htonl
		    Perl_my_memcmp
		    Perl_my_memset
		    Perl_my_ntohl
		    Perl_my_swap
		    Perl_safexcalloc
		    Perl_safexfree
		    Perl_safexmalloc
		    Perl_safexrealloc
		    Perl_unlnk
		    Perl_sys_intern_clear
		    Perl_sys_intern_init
		    )];
}


unless ($define{'DEBUGGING'}) {
    skip_symbols [qw(
		    Perl_deb_growlevel
		    Perl_debop
		    Perl_debprofdump
		    Perl_debstack
		    Perl_debstackptrs
		    Perl_runops_debug
		    Perl_sv_peek
		    PL_block_type
		    PL_watchaddr
		    PL_watchok
		    )];
}

if ($define{'PERL_IMPLICIT_SYS'}) {
    skip_symbols [qw(
		    Perl_getenv_len
		    Perl_my_popen
		    Perl_my_pclose
		    )];
}
else {
    skip_symbols [qw(
		    PL_Mem
		    PL_MemShared
		    PL_MemParse
		    PL_Env
		    PL_StdIO
		    PL_LIO
		    PL_Dir
		    PL_Sock
		    PL_Proc
		    )];
}

unless ($define{'PERL_FLEXIBLE_EXCEPTIONS'}) {
    skip_symbols [qw(
		    PL_protect
		    Perl_default_protect
		    Perl_vdefault_protect
		    )];
}

if ($define{'MYMALLOC'}) {
    emit_symbols [qw(
		    Perl_dump_mstats
		    Perl_get_mstats
		    Perl_malloc
		    Perl_mfree
		    Perl_realloc
		    Perl_calloc
		    Perl_strdup
		    Perl_putenv
		    )];
    if ($define{'USE_5005THREADS'} || $define{'USE_ITHREADS'}) {
	emit_symbols [qw(
			PL_malloc_mutex
			)];
    }
    else {
	skip_symbols [qw(
			PL_malloc_mutex
			)];
    }
}
else {
    skip_symbols [qw(
		    PL_malloc_mutex
		    Perl_dump_mstats
		    Perl_get_mstats
		    Perl_malloc
		    Perl_mfree
		    Perl_realloc
		    Perl_calloc
		    Perl_malloced_size
		    )];
}

unless ($define{'USE_5005THREADS'} || $define{'USE_ITHREADS'}) {
    skip_symbols [qw(
		    PL_thr_key
		    )];
}

unless ($define{'USE_5005THREADS'}) {
    skip_symbols [qw(
		    PL_sv_mutex
		    PL_strtab_mutex
		    PL_svref_mutex
		    PL_cred_mutex
		    PL_eval_mutex
		    PL_fdpid_mutex
		    PL_sv_lock_mutex
		    PL_eval_cond
		    PL_eval_owner
		    PL_threads_mutex
		    PL_nthreads
		    PL_nthreads_cond
		    PL_threadnum
		    PL_threadsv_names
		    PL_thrsv
		    PL_vtbl_mutex
		    Perl_condpair_magic
		    Perl_new_struct_thread
		    Perl_per_thread_magicals
		    Perl_thread_create
		    Perl_find_threadsv
		    Perl_unlock_condpair
		    Perl_magic_mutexfree
		    Perl_sv_lock
		    )];
}

unless ($define{'USE_ITHREADS'}) {
    skip_symbols [qw(
		    PL_ptr_table
		    PL_op_mutex
		    Perl_dirp_dup
		    Perl_cx_dup
		    Perl_si_dup
		    Perl_any_dup
		    Perl_ss_dup
		    Perl_fp_dup
		    Perl_gp_dup
		    Perl_he_dup
		    Perl_mg_dup
		    Perl_re_dup
		    Perl_sv_dup
		    Perl_sys_intern_dup
		    Perl_ptr_table_fetch
		    Perl_ptr_table_new
		    Perl_ptr_table_split
		    Perl_ptr_table_store
		    Perl_ptr_table_clear
		    Perl_ptr_table_free
		    perl_clone
		    perl_clone_using
		    )];
}

unless ($define{'PERL_IMPLICIT_CONTEXT'}) {
    skip_symbols [qw(
		    Perl_croak_nocontext
		    Perl_die_nocontext
		    Perl_deb_nocontext
		    Perl_form_nocontext
		    Perl_load_module_nocontext
		    Perl_mess_nocontext
		    Perl_warn_nocontext
		    Perl_warner_nocontext
		    Perl_newSVpvf_nocontext
		    Perl_sv_catpvf_nocontext
		    Perl_sv_setpvf_nocontext
		    Perl_sv_catpvf_mg_nocontext
		    Perl_sv_setpvf_mg_nocontext
		    )];
}

unless ($define{'PERL_IMPLICIT_SYS'}) {
    skip_symbols [qw(
		    perl_alloc_using
		    perl_clone_using
		    )];
}

unless ($define{'FAKE_THREADS'}) {
    skip_symbols [qw(PL_curthr)];
}

sub readvar {
    my $file = shift;
    my $proc = shift || sub { "PL_$_[2]" };
    open(VARS,$file) || die "Cannot open $file: $!\n";
    my @syms;
    while (<VARS>) {
	# All symbols have a Perl_ prefix because that's what embed.h
	# sticks in front of them.
	push(@syms, &$proc($1,$2,$3)) if (/\bPERLVAR(A?I?C?)\(([IGT])(\w+)/);
    } 
    close(VARS); 
    return \@syms;
}

if ($define{'USE_5005THREADS'}) {
    my $thrd = readvar($thrdvar_h);
    skip_symbols $thrd;
}

if ($define{'PERL_GLOBAL_STRUCT'}) {
    my $global = readvar($perlvars_h);
    skip_symbols $global;
    emit_symbol('Perl_GetVars');
    emit_symbols [qw(PL_Vars PL_VarsPtr)] unless $CCTYPE eq 'GCC';
}

# functions from *.sym files

my @syms = ($global_sym, $globvar_sym); # $pp_sym is not part of the API

if ($define{'USE_PERLIO'}) {
    push @syms, $perlio_sym;
    if ($define{'USE_SFIO'}) {
	# SFIO defines most of the PerlIO routines as macros
	skip_symbols [qw(
			 PerlIO_canset_cnt
			 PerlIO_clearerr
			 PerlIO_close
			 PerlIO_eof
			 PerlIO_error
			 PerlIO_exportFILE
			 PerlIO_fast_gets
			 PerlIO_fdopen
			 PerlIO_fileno
			 PerlIO_findFILE
			 PerlIO_flush
			 PerlIO_get_base
			 PerlIO_get_bufsiz
			 PerlIO_get_cnt
			 PerlIO_get_ptr
			 PerlIO_getc
			 PerlIO_getname
			 PerlIO_has_base
			 PerlIO_has_cntptr
			 PerlIO_importFILE
			 PerlIO_open
			 PerlIO_printf
			 PerlIO_putc
			 PerlIO_puts
			 PerlIO_read
			 PerlIO_releaseFILE
			 PerlIO_reopen
			 PerlIO_rewind
			 PerlIO_seek
			 PerlIO_set_cnt
			 PerlIO_set_ptrcnt
			 PerlIO_setlinebuf
			 PerlIO_sprintf
			 PerlIO_stderr
			 PerlIO_stdin
			 PerlIO_stdout
			 PerlIO_stdoutf
			 PerlIO_tell
			 PerlIO_ungetc
			 PerlIO_vprintf
			 PerlIO_write
			 )];
    }
}

for my $syms (@syms) {
    open (GLOBAL, "<$syms") || die "failed to open $syms: $!\n";
    while (<GLOBAL>) {
	next if (!/^[A-Za-z]/);
	# Functions have a Perl_ prefix
	# Variables have a PL_ prefix
	chomp($_);
	my $symbol = ($syms =~ /var\.sym$/i ? "PL_" : "");
	$symbol .= $_;
	emit_symbol($symbol) unless exists $skip{$symbol};
    }
    close(GLOBAL);
}

# variables

if ($define{'PERL_OBJECT'} || $define{'MULTIPLICITY'}) {
    for my $f ($perlvars_h, $intrpvar_h, $thrdvar_h) {
	my $glob = readvar($f, sub { "Perl_" . $_[1] . $_[2] . "_ptr" });
	emit_symbols $glob;
    }
    # XXX AIX seems to want the perlvars.h symbols, for some reason
    if ($PLATFORM eq 'aix') {
	my $glob = readvar($perlvars_h);
	emit_symbols $glob;
    }
}
else {
    unless ($define{'PERL_GLOBAL_STRUCT'}) {
	my $glob = readvar($perlvars_h);
	emit_symbols $glob;
    } 
    unless ($define{'MULTIPLICITY'}) {
	my $glob = readvar($intrpvar_h);
	emit_symbols $glob;
    } 
    unless ($define{'MULTIPLICITY'} || $define{'USE_5005THREADS'}) {
	my $glob = readvar($thrdvar_h);
	emit_symbols $glob;
    } 
}

sub try_symbol {
    my $symbol = shift;

    return if $symbol !~ /^[A-Za-z]/;
    return if $symbol =~ /^\#/;
    $symbol =~s/\r//g;
    chomp($symbol);
    return if exists $skip{$symbol};
    emit_symbol($symbol);
}

while (<DATA>) {
    try_symbol($_);
}

if ($PLATFORM eq 'win32') {
    foreach my $symbol (qw(
			    boot_DynaLoader
			    Perl_init_os_extras
			    Perl_thread_create
			    Perl_win32_init
			    RunPerl
			    win32_errno
			    win32_environ
			    win32_stdin
			    win32_stdout
			    win32_stderr
			    win32_ferror
			    win32_feof
			    win32_strerror
			    win32_fprintf
			    win32_printf
			    win32_vfprintf
			    win32_vprintf
			    win32_fread
			    win32_fwrite
			    win32_fopen
			    win32_fdopen
			    win32_freopen
			    win32_fclose
			    win32_fputs
			    win32_fputc
			    win32_ungetc
			    win32_getc
			    win32_fileno
			    win32_clearerr
			    win32_fflush
			    win32_ftell
			    win32_fseek
			    win32_fgetpos
			    win32_fsetpos
			    win32_rewind
			    win32_tmpfile
			    win32_abort
			    win32_fstat
			    win32_stat
			    win32_pipe
			    win32_popen
			    win32_pclose
			    win32_rename
			    win32_setmode
			    win32_lseek
			    win32_tell
			    win32_dup
			    win32_dup2
			    win32_open
			    win32_close
			    win32_eof
			    win32_read
			    win32_write
			    win32_spawnvp
			    win32_mkdir
			    win32_rmdir
			    win32_chdir
			    win32_flock
			    win32_execv
			    win32_execvp
			    win32_htons
			    win32_ntohs
			    win32_htonl
			    win32_ntohl
			    win32_inet_addr
			    win32_inet_ntoa
			    win32_socket
			    win32_bind
			    win32_listen
			    win32_accept
			    win32_connect
			    win32_send
			    win32_sendto
			    win32_recv
			    win32_recvfrom
			    win32_shutdown
			    win32_closesocket
			    win32_ioctlsocket
			    win32_setsockopt
			    win32_getsockopt
			    win32_getpeername
			    win32_getsockname
			    win32_gethostname
			    win32_gethostbyname
			    win32_gethostbyaddr
			    win32_getprotobyname
			    win32_getprotobynumber
			    win32_getservbyname
			    win32_getservbyport
			    win32_select
			    win32_endhostent
			    win32_endnetent
			    win32_endprotoent
			    win32_endservent
			    win32_getnetent
			    win32_getnetbyname
			    win32_getnetbyaddr
			    win32_getprotoent
			    win32_getservent
			    win32_sethostent
			    win32_setnetent
			    win32_setprotoent
			    win32_setservent
			    win32_getenv
			    win32_putenv
			    win32_perror
			    win32_setbuf
			    win32_setvbuf
			    win32_flushall
			    win32_fcloseall
			    win32_fgets
			    win32_gets
			    win32_fgetc
			    win32_putc
			    win32_puts
			    win32_getchar
			    win32_putchar
			    win32_malloc
			    win32_calloc
			    win32_realloc
			    win32_free
			    win32_sleep
			    win32_times
			    win32_access
			    win32_alarm
			    win32_chmod
			    win32_open_osfhandle
			    win32_get_osfhandle
			    win32_ioctl
			    win32_link
			    win32_unlink
			    win32_utime
			    win32_uname
			    win32_wait
			    win32_waitpid
			    win32_kill
			    win32_str_os_error
			    win32_opendir
			    win32_readdir
			    win32_telldir
			    win32_seekdir
			    win32_rewinddir
			    win32_closedir
			    win32_longpath
			    win32_os_id
			    win32_getpid
			    win32_crypt
			    win32_dynaload
			   ))
    {
	try_symbol($symbol);
    }
}
elsif ($PLATFORM eq 'os2') {
    open MAP, 'miniperl.map' or die 'Cannot read miniperl.map';
    /^\s*[\da-f:]+\s+(\w+)/i and $mapped{$1}++ foreach <MAP>;
    close MAP or die 'Cannot close miniperl.map';

    @missing = grep { !exists $mapped{$_} and !exists $bincompat5005{$_} }
		    keys %export;
    delete $export{$_} foreach @missing;
}
elsif ($PLATFORM eq 'MacOS') {
    open MACSYMS, 'macperl.sym' or die 'Cannot read macperl.sym';

    while (<MACSYMS>) {
	try_symbol($_);
    }

    close MACSYMS;
}

# Now all symbols should be defined because
# next we are going to output them.

foreach my $symbol (sort keys %export) {
    output_symbol($symbol);
}

sub emit_symbol {
    my $symbol = shift;
    chomp($symbol); 
    $export{$symbol} = 1;
}

sub output_symbol {
    my $symbol = shift;
    $symbol = $bincompat5005{$symbol}
	if $define{PERL_BINCOMPAT_5005} and $symbol =~ /^($bincompat5005)$/;
    if ($PLATFORM eq 'win32') {
	$symbol = "_$symbol" if $CCTYPE eq 'BORLAND';
	print "\t$symbol\n";
# XXX: binary compatibility between compilers is an exercise
# in frustration :-(
#        if ($CCTYPE eq "BORLAND") {
#	    # workaround Borland quirk by exporting both the straight
#	    # name and a name with leading underscore.  Note the
#	    # alias *must* come after the symbol itself, if both
#	    # are to be exported. (Linker bug?)
#	    print "\t_$symbol\n";
#	    print "\t$symbol = _$symbol\n";
#	}
#	elsif ($CCTYPE eq 'GCC') {
#	    # Symbols have leading _ whole process is $%@"% slow
#	    # so skip aliases for now
#	    nprint "\t$symbol\n";
#	}
#	else {
#	    # for binary coexistence, export both the symbol and
#	    # alias with leading underscore
#	    print "\t$symbol\n";
#	    print "\t_$symbol = $symbol\n";
#	}
    }
    elsif ($PLATFORM eq 'os2') {
	print qq(    "$symbol"\n);
    }
    elsif ($PLATFORM eq 'aix' || $PLATFORM eq 'MacOS') {
	print "$symbol\n";
    }
}

1;
__DATA__
# extra globals not included above.
perl_alloc
perl_alloc_using
perl_clone
perl_clone_using
perl_construct
perl_destruct
perl_free
perl_parse
perl_run
