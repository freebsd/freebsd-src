#include "INTERN.h"
#include "perl.h"

#ifdef PERL_OBJECT
#undef  pp_null
#define pp_null		CPerlObj::Perl_pp_null
#undef  pp_stub		
#define pp_stub		CPerlObj::Perl_pp_stub
#undef  pp_scalar	
#define pp_scalar	CPerlObj::Perl_pp_scalar
#undef  pp_pushmark	
#define pp_pushmark	CPerlObj::Perl_pp_pushmark
#undef  pp_wantarray	
#define pp_wantarray	CPerlObj::Perl_pp_wantarray
#undef  pp_const	
#define pp_const	CPerlObj::Perl_pp_const
#undef  pp_gvsv		
#define pp_gvsv		CPerlObj::Perl_pp_gvsv	
#undef  pp_gv		
#define pp_gv		CPerlObj::Perl_pp_gv	
#undef  pp_gelem	
#define pp_gelem	CPerlObj::Perl_pp_gelem
#undef  pp_padsv	
#define pp_padsv	CPerlObj::Perl_pp_padsv
#undef  pp_padav	
#define pp_padav	CPerlObj::Perl_pp_padav
#undef  pp_padhv	
#define pp_padhv	CPerlObj::Perl_pp_padhv
#undef  pp_padany	
#define pp_padany	CPerlObj::Perl_pp_padany
#undef  pp_pushre	
#define pp_pushre	CPerlObj::Perl_pp_pushre
#undef  pp_rv2gv	
#define pp_rv2gv	CPerlObj::Perl_pp_rv2gv
#undef  pp_rv2sv	
#define pp_rv2sv	CPerlObj::Perl_pp_rv2sv
#undef  pp_av2arylen	
#define pp_av2arylen	CPerlObj::Perl_pp_av2arylen
#undef  pp_rv2cv	
#define pp_rv2cv	CPerlObj::Perl_pp_rv2cv
#undef  pp_anoncode	
#define pp_anoncode	CPerlObj::Perl_pp_anoncode
#undef  pp_prototype	
#define pp_prototype	CPerlObj::Perl_pp_prototype
#undef  pp_refgen	
#define pp_refgen	CPerlObj::Perl_pp_refgen
#undef  pp_srefgen	
#define pp_srefgen	CPerlObj::Perl_pp_srefgen
#undef  pp_ref		
#define pp_ref		CPerlObj::Perl_pp_ref	
#undef  pp_bless	
#define pp_bless	CPerlObj::Perl_pp_bless
#undef  pp_backtick	
#define pp_backtick	CPerlObj::Perl_pp_backtick
#undef  pp_glob		
#define pp_glob		CPerlObj::Perl_pp_glob	
#undef  pp_readline	
#define pp_readline	CPerlObj::Perl_pp_readline
#undef  pp_rcatline	
#define pp_rcatline	CPerlObj::Perl_pp_rcatline
#undef  pp_regcmaybe	
#define pp_regcmaybe	CPerlObj::Perl_pp_regcmaybe
#undef  pp_regcreset	
#define pp_regcreset	CPerlObj::Perl_pp_regcreset
#undef  pp_regcomp	
#define pp_regcomp	CPerlObj::Perl_pp_regcomp
#undef  pp_match	
#define pp_match	CPerlObj::Perl_pp_match
#undef  pp_qr
#define pp_qr		CPerlObj::Perl_pp_qr
#undef  pp_subst	
#define pp_subst	CPerlObj::Perl_pp_subst
#undef  pp_substcont	
#define pp_substcont	CPerlObj::Perl_pp_substcont
#undef  pp_trans	
#define pp_trans	CPerlObj::Perl_pp_trans
#undef  pp_sassign	
#define pp_sassign	CPerlObj::Perl_pp_sassign
#undef  pp_aassign	
#define pp_aassign	CPerlObj::Perl_pp_aassign
#undef  pp_chop		
#define pp_chop		CPerlObj::Perl_pp_chop	
#undef  pp_schop	
#define pp_schop	CPerlObj::Perl_pp_schop
#undef  pp_chomp	
#define pp_chomp	CPerlObj::Perl_pp_chomp
#undef  pp_schomp	
#define pp_schomp	CPerlObj::Perl_pp_schomp
#undef  pp_defined	
#define pp_defined	CPerlObj::Perl_pp_defined
#undef  pp_undef	
#define pp_undef	CPerlObj::Perl_pp_undef
#undef  pp_study	
#define pp_study	CPerlObj::Perl_pp_study
#undef  pp_pos		
#define pp_pos		CPerlObj::Perl_pp_pos	
#undef  pp_preinc	
#define pp_preinc	CPerlObj::Perl_pp_preinc
#undef  pp_i_preinc	
#define pp_i_preinc	CPerlObj::Perl_pp_preinc
#undef  pp_predec	
#define pp_predec	CPerlObj::Perl_pp_predec
#undef  pp_i_predec	
#define pp_i_predec	CPerlObj::Perl_pp_predec
#undef  pp_postinc	
#define pp_postinc	CPerlObj::Perl_pp_postinc
#undef  pp_i_postinc	
#define pp_i_postinc	CPerlObj::Perl_pp_postinc
#undef  pp_postdec	
#define pp_postdec	CPerlObj::Perl_pp_postdec
#undef  pp_i_postdec	
#define pp_i_postdec	CPerlObj::Perl_pp_postdec
#undef  pp_pow		
#define pp_pow		CPerlObj::Perl_pp_pow	
#undef  pp_multiply	
#define pp_multiply	CPerlObj::Perl_pp_multiply
#undef  pp_i_multiply	
#define pp_i_multiply	CPerlObj::Perl_pp_i_multiply
#undef  pp_divide	
#define pp_divide	CPerlObj::Perl_pp_divide
#undef  pp_i_divide	
#define pp_i_divide	CPerlObj::Perl_pp_i_divide
#undef  pp_modulo	
#define pp_modulo	CPerlObj::Perl_pp_modulo
#undef  pp_i_modulo	
#define pp_i_modulo	CPerlObj::Perl_pp_i_modulo
#undef  pp_repeat	
#define pp_repeat	CPerlObj::Perl_pp_repeat
#undef  pp_add		
#define pp_add		CPerlObj::Perl_pp_add	
#undef  pp_i_add	
#define pp_i_add	CPerlObj::Perl_pp_i_add
#undef  pp_subtract	
#define pp_subtract	CPerlObj::Perl_pp_subtract
#undef  pp_i_subtract	
#define pp_i_subtract	CPerlObj::Perl_pp_i_subtract
#undef  pp_concat	
#define pp_concat	CPerlObj::Perl_pp_concat
#undef  pp_stringify	
#define pp_stringify	CPerlObj::Perl_pp_stringify
#undef  pp_left_shift	
#define pp_left_shift	CPerlObj::Perl_pp_left_shift
#undef  pp_right_shift	
#define pp_right_shift	CPerlObj::Perl_pp_right_shift
#undef  pp_lt		
#define pp_lt		CPerlObj::Perl_pp_lt	
#undef  pp_i_lt		
#define pp_i_lt		CPerlObj::Perl_pp_i_lt	
#undef  pp_gt		
#define pp_gt		CPerlObj::Perl_pp_gt	
#undef  pp_i_gt		
#define pp_i_gt		CPerlObj::Perl_pp_i_gt	
#undef  pp_le		
#define pp_le		CPerlObj::Perl_pp_le	
#undef  pp_i_le		
#define pp_i_le		CPerlObj::Perl_pp_i_le	
#undef  pp_ge		
#define pp_ge		CPerlObj::Perl_pp_ge	
#undef  pp_i_ge		
#define pp_i_ge		CPerlObj::Perl_pp_i_ge	
#undef  pp_eq		
#define pp_eq		CPerlObj::Perl_pp_eq	
#undef  pp_i_eq		
#define pp_i_eq		CPerlObj::Perl_pp_i_eq	
#undef  pp_ne		
#define pp_ne		CPerlObj::Perl_pp_ne	
#undef  pp_i_ne		
#define pp_i_ne		CPerlObj::Perl_pp_i_ne	
#undef  pp_ncmp		
#define pp_ncmp		CPerlObj::Perl_pp_ncmp	
#undef  pp_i_ncmp	
#define pp_i_ncmp	CPerlObj::Perl_pp_i_ncmp
#undef  pp_slt		
#define pp_slt		CPerlObj::Perl_pp_slt	
#undef  pp_sgt		
#define pp_sgt		CPerlObj::Perl_pp_sgt	
#undef  pp_sle		
#define pp_sle		CPerlObj::Perl_pp_sle	
#undef  pp_sge		
#define pp_sge		CPerlObj::Perl_pp_sge	
#undef  pp_seq		
#define pp_seq		CPerlObj::Perl_pp_seq	
#undef  pp_sne		
#define pp_sne		CPerlObj::Perl_pp_sne	
#undef  pp_scmp		
#define pp_scmp		CPerlObj::Perl_pp_scmp	
#undef  pp_bit_and	
#define pp_bit_and	CPerlObj::Perl_pp_bit_and
#undef  pp_bit_xor	
#define pp_bit_xor	CPerlObj::Perl_pp_bit_xor
#undef  pp_bit_or	
#define pp_bit_or	CPerlObj::Perl_pp_bit_or
#undef  pp_negate	
#define pp_negate	CPerlObj::Perl_pp_negate
#undef  pp_i_negate	
#define pp_i_negate	CPerlObj::Perl_pp_i_negate
#undef  pp_not		
#define pp_not		CPerlObj::Perl_pp_not	
#undef  pp_complement	
#define pp_complement	CPerlObj::Perl_pp_complement
#undef  pp_atan2	
#define pp_atan2	CPerlObj::Perl_pp_atan2
#undef  pp_sin		
#define pp_sin		CPerlObj::Perl_pp_sin	
#undef  pp_cos		
#define pp_cos		CPerlObj::Perl_pp_cos	
#undef  pp_rand		
#define pp_rand		CPerlObj::Perl_pp_rand	
#undef  pp_srand	
#define pp_srand	CPerlObj::Perl_pp_srand
#undef  pp_exp		
#define pp_exp		CPerlObj::Perl_pp_exp	
#undef  pp_log		
#define pp_log		CPerlObj::Perl_pp_log	
#undef  pp_sqrt		
#define pp_sqrt		CPerlObj::Perl_pp_sqrt	
#undef  pp_int		
#define pp_int		CPerlObj::Perl_pp_int	
#undef  pp_hex		
#define pp_hex		CPerlObj::Perl_pp_hex	
#undef  pp_oct		
#define pp_oct		CPerlObj::Perl_pp_oct	
#undef  pp_abs		
#define pp_abs		CPerlObj::Perl_pp_abs	
#undef  pp_length	
#define pp_length	CPerlObj::Perl_pp_length
#undef  pp_substr	
#define pp_substr	CPerlObj::Perl_pp_substr
#undef  pp_vec		
#define pp_vec		CPerlObj::Perl_pp_vec	
#undef  pp_index	
#define pp_index	CPerlObj::Perl_pp_index
#undef  pp_rindex	
#define pp_rindex	CPerlObj::Perl_pp_rindex
#undef  pp_sprintf	
#define pp_sprintf	CPerlObj::Perl_pp_sprintf
#undef  pp_formline	
#define pp_formline	CPerlObj::Perl_pp_formline
#undef  pp_ord		
#define pp_ord		CPerlObj::Perl_pp_ord	
#undef  pp_chr		
#define pp_chr		CPerlObj::Perl_pp_chr	
#undef  pp_crypt	
#define pp_crypt	CPerlObj::Perl_pp_crypt
#undef  pp_ucfirst	
#define pp_ucfirst	CPerlObj::Perl_pp_ucfirst
#undef  pp_lcfirst	
#define pp_lcfirst	CPerlObj::Perl_pp_lcfirst
#undef  pp_uc		
#define pp_uc		CPerlObj::Perl_pp_uc	
#undef  pp_lc		
#define pp_lc		CPerlObj::Perl_pp_lc	
#undef  pp_quotemeta	
#define pp_quotemeta	CPerlObj::Perl_pp_quotemeta
#undef  pp_rv2av	
#define pp_rv2av	CPerlObj::Perl_pp_rv2av
#undef  pp_aelemfast	
#define pp_aelemfast	CPerlObj::Perl_pp_aelemfast
#undef  pp_aelem	
#define pp_aelem	CPerlObj::Perl_pp_aelem
#undef  pp_aslice	
#define pp_aslice	CPerlObj::Perl_pp_aslice
#undef  pp_each		
#define pp_each		CPerlObj::Perl_pp_each	
#undef  pp_values	
#define pp_values	CPerlObj::Perl_pp_values
#undef  pp_keys		
#define pp_keys		CPerlObj::Perl_pp_keys	
#undef  pp_delete	
#define pp_delete	CPerlObj::Perl_pp_delete
#undef  pp_exists	
#define pp_exists	CPerlObj::Perl_pp_exists
#undef  pp_rv2hv	
#define pp_rv2hv	CPerlObj::Perl_pp_rv2hv
#undef  pp_helem	
#define pp_helem	CPerlObj::Perl_pp_helem
#undef  pp_hslice	
#define pp_hslice	CPerlObj::Perl_pp_hslice
#undef  pp_unpack	
#define pp_unpack	CPerlObj::Perl_pp_unpack
#undef  pp_pack		
#define pp_pack		CPerlObj::Perl_pp_pack	
#undef  pp_split	
#define pp_split	CPerlObj::Perl_pp_split
#undef  pp_join		
#define pp_join		CPerlObj::Perl_pp_join	
#undef  pp_list		
#define pp_list		CPerlObj::Perl_pp_list	
#undef  pp_lslice	
#define pp_lslice	CPerlObj::Perl_pp_lslice
#undef  pp_anonlist	
#define pp_anonlist	CPerlObj::Perl_pp_anonlist
#undef  pp_anonhash	
#define pp_anonhash	CPerlObj::Perl_pp_anonhash
#undef  pp_splice	
#define pp_splice	CPerlObj::Perl_pp_splice
#undef  pp_push		
#define pp_push		CPerlObj::Perl_pp_push	
#undef  pp_pop		
#define pp_pop		CPerlObj::Perl_pp_pop	
#undef  pp_shift	
#define pp_shift	CPerlObj::Perl_pp_shift
#undef  pp_unshift	
#define pp_unshift	CPerlObj::Perl_pp_unshift
#undef  pp_sort		
#define pp_sort		CPerlObj::Perl_pp_sort	
#undef  pp_reverse	
#define pp_reverse	CPerlObj::Perl_pp_reverse
#undef  pp_grepstart	
#define pp_grepstart	CPerlObj::Perl_pp_grepstart
#undef  pp_grepwhile	
#define pp_grepwhile	CPerlObj::Perl_pp_grepwhile
#undef  pp_mapstart	
#define pp_mapstart	CPerlObj::Perl_pp_mapstart
#undef  pp_mapwhile	
#define pp_mapwhile	CPerlObj::Perl_pp_mapwhile
#undef  pp_range	
#define pp_range	CPerlObj::Perl_pp_range
#undef  pp_flip		
#define pp_flip		CPerlObj::Perl_pp_flip	
#undef  pp_flop		
#define pp_flop		CPerlObj::Perl_pp_flop	
#undef  pp_and		
#define pp_and		CPerlObj::Perl_pp_and	
#undef  pp_or		
#define pp_or		CPerlObj::Perl_pp_or	
#undef  pp_xor		
#define pp_xor		CPerlObj::Perl_pp_xor	
#undef  pp_cond_expr	
#define pp_cond_expr	CPerlObj::Perl_pp_cond_expr
#undef  pp_andassign	
#define pp_andassign	CPerlObj::Perl_pp_andassign
#undef  pp_orassign	
#define pp_orassign	CPerlObj::Perl_pp_orassign
#undef  pp_method	
#define pp_method	CPerlObj::Perl_pp_method
#undef  pp_entersub	
#define pp_entersub	CPerlObj::Perl_pp_entersub
#undef  pp_leavesub	
#define pp_leavesub	CPerlObj::Perl_pp_leavesub
#undef  pp_caller	
#define pp_caller	CPerlObj::Perl_pp_caller
#undef  pp_warn		
#define pp_warn		CPerlObj::Perl_pp_warn	
#undef  pp_die		
#define pp_die		CPerlObj::Perl_pp_die	
#undef  pp_reset	
#define pp_reset	CPerlObj::Perl_pp_reset
#undef  pp_lineseq	
#define pp_lineseq	CPerlObj::Perl_pp_lineseq
#undef  pp_nextstate	
#define pp_nextstate	CPerlObj::Perl_pp_nextstate
#undef  pp_dbstate	
#define pp_dbstate	CPerlObj::Perl_pp_dbstate
#undef  pp_unstack	
#define pp_unstack	CPerlObj::Perl_pp_unstack
#undef  pp_enter	
#define pp_enter	CPerlObj::Perl_pp_enter
#undef  pp_leave	
#define pp_leave	CPerlObj::Perl_pp_leave
#undef  pp_scope	
#define pp_scope	CPerlObj::Perl_pp_scope
#undef  pp_enteriter	
#define pp_enteriter	CPerlObj::Perl_pp_enteriter
#undef  pp_iter		
#define pp_iter		CPerlObj::Perl_pp_iter	
#undef  pp_enterloop	
#define pp_enterloop	CPerlObj::Perl_pp_enterloop
#undef  pp_leaveloop	
#define pp_leaveloop	CPerlObj::Perl_pp_leaveloop
#undef  pp_return	
#define pp_return	CPerlObj::Perl_pp_return
#undef  pp_last		
#define pp_last		CPerlObj::Perl_pp_last	
#undef  pp_next		
#define pp_next		CPerlObj::Perl_pp_next	
#undef  pp_redo		
#define pp_redo		CPerlObj::Perl_pp_redo	
#undef  pp_dump		
#define pp_dump		CPerlObj::Perl_pp_dump	
#undef  pp_goto		
#define pp_goto		CPerlObj::Perl_pp_goto	
#undef  pp_exit		
#define pp_exit		CPerlObj::Perl_pp_exit	
#undef  pp_open		
#define pp_open		CPerlObj::Perl_pp_open	
#undef  pp_close	
#define pp_close	CPerlObj::Perl_pp_close
#undef  pp_pipe_op	
#define pp_pipe_op	CPerlObj::Perl_pp_pipe_op
#undef  pp_fileno	
#define pp_fileno	CPerlObj::Perl_pp_fileno
#undef  pp_umask	
#define pp_umask	CPerlObj::Perl_pp_umask
#undef  pp_binmode	
#define pp_binmode	CPerlObj::Perl_pp_binmode
#undef  pp_tie		
#define pp_tie		CPerlObj::Perl_pp_tie	
#undef  pp_untie	
#define pp_untie	CPerlObj::Perl_pp_untie
#undef  pp_tied		
#define pp_tied		CPerlObj::Perl_pp_tied	
#undef  pp_dbmopen	
#define pp_dbmopen	CPerlObj::Perl_pp_dbmopen
#undef  pp_dbmclose	
#define pp_dbmclose	CPerlObj::Perl_pp_dbmclose
#undef  pp_sselect	
#define pp_sselect	CPerlObj::Perl_pp_sselect
#undef  pp_select	
#define pp_select	CPerlObj::Perl_pp_select
#undef  pp_getc		
#define pp_getc		CPerlObj::Perl_pp_getc	
#undef  pp_read		
#define pp_read		CPerlObj::Perl_pp_read	
#undef  pp_enterwrite	
#define pp_enterwrite	CPerlObj::Perl_pp_enterwrite
#undef  pp_leavewrite	
#define pp_leavewrite	CPerlObj::Perl_pp_leavewrite
#undef  pp_prtf		
#define pp_prtf		CPerlObj::Perl_pp_prtf	
#undef  pp_print	
#define pp_print	CPerlObj::Perl_pp_print
#undef  pp_sysopen	
#define pp_sysopen	CPerlObj::Perl_pp_sysopen
#undef  pp_sysseek	
#define pp_sysseek	CPerlObj::Perl_pp_sysseek
#undef  pp_sysread	
#define pp_sysread	CPerlObj::Perl_pp_sysread
#undef  pp_syswrite	
#define pp_syswrite	CPerlObj::Perl_pp_syswrite
#undef  pp_send		
#define pp_send		CPerlObj::Perl_pp_send	
#undef  pp_recv		
#define pp_recv		CPerlObj::Perl_pp_recv	
#undef  pp_eof		
#define pp_eof		CPerlObj::Perl_pp_eof	
#undef  pp_tell		
#define pp_tell		CPerlObj::Perl_pp_tell	
#undef  pp_seek		
#define pp_seek		CPerlObj::Perl_pp_seek	
#undef  pp_truncate	
#define pp_truncate	CPerlObj::Perl_pp_truncate
#undef  pp_fcntl	
#define pp_fcntl	CPerlObj::Perl_pp_fcntl
#undef  pp_ioctl	
#define pp_ioctl	CPerlObj::Perl_pp_ioctl
#undef  pp_flock	
#define pp_flock	CPerlObj::Perl_pp_flock
#undef  pp_socket	
#define pp_socket	CPerlObj::Perl_pp_socket
#undef  pp_sockpair	
#define pp_sockpair	CPerlObj::Perl_pp_sockpair
#undef  pp_bind		
#define pp_bind		CPerlObj::Perl_pp_bind	
#undef  pp_connect	
#define pp_connect	CPerlObj::Perl_pp_connect
#undef  pp_listen	
#define pp_listen	CPerlObj::Perl_pp_listen
#undef  pp_accept	
#define pp_accept	CPerlObj::Perl_pp_accept
#undef  pp_shutdown	
#define pp_shutdown	CPerlObj::Perl_pp_shutdown
#undef  pp_gsockopt	
#define pp_gsockopt	CPerlObj::Perl_pp_gsockopt
#undef  pp_ssockopt	
#define pp_ssockopt	CPerlObj::Perl_pp_ssockopt
#undef  pp_getsockname	
#define pp_getsockname	CPerlObj::Perl_pp_getsockname
#undef  pp_getpeername	
#define pp_getpeername	CPerlObj::Perl_pp_getpeername
#undef  pp_lstat	
#define pp_lstat	CPerlObj::Perl_pp_lstat
#undef  pp_stat		
#define pp_stat		CPerlObj::Perl_pp_stat	
#undef  pp_ftrread	
#define pp_ftrread	CPerlObj::Perl_pp_ftrread
#undef  pp_ftrwrite	
#define pp_ftrwrite	CPerlObj::Perl_pp_ftrwrite
#undef  pp_ftrexec	
#define pp_ftrexec	CPerlObj::Perl_pp_ftrexec
#undef  pp_fteread	
#define pp_fteread	CPerlObj::Perl_pp_fteread
#undef  pp_ftewrite	
#define pp_ftewrite	CPerlObj::Perl_pp_ftewrite
#undef  pp_fteexec	
#define pp_fteexec	CPerlObj::Perl_pp_fteexec
#undef  pp_ftis		
#define pp_ftis		CPerlObj::Perl_pp_ftis	
#undef  pp_fteowned	
#define pp_fteowned	CPerlObj::Perl_pp_fteowned
#undef  pp_ftrowned	
#define pp_ftrowned	CPerlObj::Perl_pp_ftrowned
#undef  pp_ftzero	
#define pp_ftzero	CPerlObj::Perl_pp_ftzero
#undef  pp_ftsize	
#define pp_ftsize	CPerlObj::Perl_pp_ftsize
#undef  pp_ftmtime	
#define pp_ftmtime	CPerlObj::Perl_pp_ftmtime
#undef  pp_ftatime	
#define pp_ftatime	CPerlObj::Perl_pp_ftatime
#undef  pp_ftctime	
#define pp_ftctime	CPerlObj::Perl_pp_ftctime
#undef  pp_ftsock	
#define pp_ftsock	CPerlObj::Perl_pp_ftsock
#undef  pp_ftchr	
#define pp_ftchr	CPerlObj::Perl_pp_ftchr
#undef  pp_ftblk	
#define pp_ftblk	CPerlObj::Perl_pp_ftblk
#undef  pp_ftfile	
#define pp_ftfile	CPerlObj::Perl_pp_ftfile
#undef  pp_ftdir	
#define pp_ftdir	CPerlObj::Perl_pp_ftdir
#undef  pp_ftpipe	
#define pp_ftpipe	CPerlObj::Perl_pp_ftpipe
#undef  pp_ftlink	
#define pp_ftlink	CPerlObj::Perl_pp_ftlink
#undef  pp_ftsuid	
#define pp_ftsuid	CPerlObj::Perl_pp_ftsuid
#undef  pp_ftsgid	
#define pp_ftsgid	CPerlObj::Perl_pp_ftsgid
#undef  pp_ftsvtx	
#define pp_ftsvtx	CPerlObj::Perl_pp_ftsvtx
#undef  pp_fttty	
#define pp_fttty	CPerlObj::Perl_pp_fttty
#undef  pp_fttext	
#define pp_fttext	CPerlObj::Perl_pp_fttext
#undef  pp_ftbinary	
#define pp_ftbinary	CPerlObj::Perl_pp_ftbinary
#undef  pp_chdir	
#define pp_chdir	CPerlObj::Perl_pp_chdir
#undef  pp_chown	
#define pp_chown	CPerlObj::Perl_pp_chown
#undef  pp_chroot	
#define pp_chroot	CPerlObj::Perl_pp_chroot
#undef  pp_unlink	
#define pp_unlink	CPerlObj::Perl_pp_unlink
#undef  pp_chmod	
#define pp_chmod	CPerlObj::Perl_pp_chmod
#undef  pp_utime	
#define pp_utime	CPerlObj::Perl_pp_utime
#undef  pp_rename	
#define pp_rename	CPerlObj::Perl_pp_rename
#undef  pp_link		
#define pp_link		CPerlObj::Perl_pp_link	
#undef  pp_symlink	
#define pp_symlink	CPerlObj::Perl_pp_symlink
#undef  pp_readlink	
#define pp_readlink	CPerlObj::Perl_pp_readlink
#undef  pp_mkdir	
#define pp_mkdir	CPerlObj::Perl_pp_mkdir
#undef  pp_rmdir	
#define pp_rmdir	CPerlObj::Perl_pp_rmdir
#undef  pp_open_dir	
#define pp_open_dir	CPerlObj::Perl_pp_open_dir
#undef  pp_readdir	
#define pp_readdir	CPerlObj::Perl_pp_readdir
#undef  pp_telldir	
#define pp_telldir	CPerlObj::Perl_pp_telldir
#undef  pp_seekdir	
#define pp_seekdir	CPerlObj::Perl_pp_seekdir
#undef  pp_rewinddir	
#define pp_rewinddir	CPerlObj::Perl_pp_rewinddir
#undef  pp_closedir	
#define pp_closedir	CPerlObj::Perl_pp_closedir
#undef  pp_fork		
#define pp_fork		CPerlObj::Perl_pp_fork	
#undef  pp_wait		
#define pp_wait		CPerlObj::Perl_pp_wait	
#undef  pp_waitpid	
#define pp_waitpid	CPerlObj::Perl_pp_waitpid
#undef  pp_system	
#define pp_system	CPerlObj::Perl_pp_system
#undef  pp_exec		
#define pp_exec		CPerlObj::Perl_pp_exec	
#undef  pp_kill		
#define pp_kill		CPerlObj::Perl_pp_kill	
#undef  pp_getppid	
#define pp_getppid	CPerlObj::Perl_pp_getppid
#undef  pp_getpgrp	
#define pp_getpgrp	CPerlObj::Perl_pp_getpgrp
#undef  pp_setpgrp	
#define pp_setpgrp	CPerlObj::Perl_pp_setpgrp
#undef  pp_getpriority	
#define pp_getpriority	CPerlObj::Perl_pp_getpriority
#undef  pp_setpriority	
#define pp_setpriority	CPerlObj::Perl_pp_setpriority
#undef  pp_time		
#define pp_time		CPerlObj::Perl_pp_time	
#undef  pp_tms		
#define pp_tms		CPerlObj::Perl_pp_tms	
#undef  pp_localtime	
#define pp_localtime	CPerlObj::Perl_pp_localtime
#undef  pp_gmtime	
#define pp_gmtime	CPerlObj::Perl_pp_gmtime
#undef  pp_alarm	
#define pp_alarm	CPerlObj::Perl_pp_alarm
#undef  pp_sleep	
#define pp_sleep	CPerlObj::Perl_pp_sleep
#undef  pp_shmget	
#define pp_shmget	CPerlObj::Perl_pp_shmget
#undef  pp_shmctl	
#define pp_shmctl	CPerlObj::Perl_pp_shmctl
#undef  pp_shmread	
#define pp_shmread	CPerlObj::Perl_pp_shmread
#undef  pp_shmwrite	
#define pp_shmwrite	CPerlObj::Perl_pp_shmwrite
#undef  pp_msgget	
#define pp_msgget	CPerlObj::Perl_pp_msgget
#undef  pp_msgctl	
#define pp_msgctl	CPerlObj::Perl_pp_msgctl
#undef  pp_msgsnd	
#define pp_msgsnd	CPerlObj::Perl_pp_msgsnd
#undef  pp_msgrcv	
#define pp_msgrcv	CPerlObj::Perl_pp_msgrcv
#undef  pp_semget	
#define pp_semget	CPerlObj::Perl_pp_semget
#undef  pp_semctl	
#define pp_semctl	CPerlObj::Perl_pp_semctl
#undef  pp_semop	
#define pp_semop	CPerlObj::Perl_pp_semop
#undef  pp_require	
#define pp_require	CPerlObj::Perl_pp_require
#undef  pp_dofile	
#define pp_dofile	CPerlObj::Perl_pp_dofile
#undef  pp_entereval	
#define pp_entereval	CPerlObj::Perl_pp_entereval
#undef  pp_leaveeval	
#define pp_leaveeval	CPerlObj::Perl_pp_leaveeval
#undef  pp_entertry	
#define pp_entertry	CPerlObj::Perl_pp_entertry
#undef  pp_leavetry	
#define pp_leavetry	CPerlObj::Perl_pp_leavetry
#undef  pp_ghbyname	
#define pp_ghbyname	CPerlObj::Perl_pp_ghbyname
#undef  pp_ghbyaddr	
#define pp_ghbyaddr	CPerlObj::Perl_pp_ghbyaddr
#undef  pp_ghostent	
#define pp_ghostent	CPerlObj::Perl_pp_ghostent
#undef  pp_gnbyname	
#define pp_gnbyname	CPerlObj::Perl_pp_gnbyname
#undef  pp_gnbyaddr	
#define pp_gnbyaddr	CPerlObj::Perl_pp_gnbyaddr
#undef  pp_gnetent	
#define pp_gnetent	CPerlObj::Perl_pp_gnetent
#undef  pp_gpbyname	
#define pp_gpbyname	CPerlObj::Perl_pp_gpbyname
#undef  pp_gpbynumber	
#define pp_gpbynumber	CPerlObj::Perl_pp_gpbynumber
#undef  pp_gprotoent	
#define pp_gprotoent	CPerlObj::Perl_pp_gprotoent
#undef  pp_gsbyname	
#define pp_gsbyname	CPerlObj::Perl_pp_gsbyname
#undef  pp_gsbyport	
#define pp_gsbyport	CPerlObj::Perl_pp_gsbyport
#undef  pp_gservent	
#define pp_gservent	CPerlObj::Perl_pp_gservent
#undef  pp_shostent	
#define pp_shostent	CPerlObj::Perl_pp_shostent
#undef  pp_snetent	
#define pp_snetent	CPerlObj::Perl_pp_snetent
#undef  pp_sprotoent	
#define pp_sprotoent	CPerlObj::Perl_pp_sprotoent
#undef  pp_sservent	
#define pp_sservent	CPerlObj::Perl_pp_sservent
#undef  pp_ehostent	
#define pp_ehostent	CPerlObj::Perl_pp_ehostent
#undef  pp_enetent	
#define pp_enetent	CPerlObj::Perl_pp_enetent
#undef  pp_eprotoent	
#define pp_eprotoent	CPerlObj::Perl_pp_eprotoent
#undef  pp_eservent	
#define pp_eservent	CPerlObj::Perl_pp_eservent
#undef  pp_gpwnam	
#define pp_gpwnam	CPerlObj::Perl_pp_gpwnam
#undef  pp_gpwuid	
#define pp_gpwuid	CPerlObj::Perl_pp_gpwuid
#undef  pp_gpwent	
#define pp_gpwent	CPerlObj::Perl_pp_gpwent
#undef  pp_spwent	
#define pp_spwent	CPerlObj::Perl_pp_spwent
#undef  pp_epwent	
#define pp_epwent	CPerlObj::Perl_pp_epwent
#undef  pp_ggrnam	
#define pp_ggrnam	CPerlObj::Perl_pp_ggrnam
#undef  pp_ggrgid	
#define pp_ggrgid	CPerlObj::Perl_pp_ggrgid
#undef  pp_ggrent	
#define pp_ggrent	CPerlObj::Perl_pp_ggrent
#undef  pp_sgrent	
#define pp_sgrent	CPerlObj::Perl_pp_sgrent
#undef  pp_egrent	
#define pp_egrent	CPerlObj::Perl_pp_egrent
#undef  pp_getlogin	
#define pp_getlogin	CPerlObj::Perl_pp_getlogin
#undef  pp_syscall	
#define pp_syscall	CPerlObj::Perl_pp_syscall
#undef  pp_lock		
#define pp_lock		CPerlObj::Perl_pp_lock	
#undef  pp_threadsv	
#define pp_threadsv	CPerlObj::Perl_pp_threadsv

OP * (CPERLscope(*check)[]) _((OP *op)) = {
	ck_null,	/* null */
	ck_null,	/* stub */
	ck_fun,		/* scalar */
	ck_null,	/* pushmark */
	ck_null,	/* wantarray */
	ck_svconst,	/* const */
	ck_null,	/* gvsv */
	ck_null,	/* gv */
	ck_null,	/* gelem */
	ck_null,	/* padsv */
	ck_null,	/* padav */
	ck_null,	/* padhv */
	ck_null,	/* padany */
	ck_null,	/* pushre */
	ck_rvconst,	/* rv2gv */
	ck_rvconst,	/* rv2sv */
	ck_null,	/* av2arylen */
	ck_rvconst,	/* rv2cv */
	ck_anoncode,	/* anoncode */
	ck_null,	/* prototype */
	ck_spair,	/* refgen */
	ck_null,	/* srefgen */
	ck_fun,		/* ref */
	ck_fun,		/* bless */
	ck_null,	/* backtick */
	ck_glob,	/* glob */
	ck_null,	/* readline */
	ck_null,	/* rcatline */
	ck_fun,		/* regcmaybe */
	ck_fun,		/* regcreset */
	ck_null,	/* regcomp */
	ck_match,	/* match */
	ck_match,	/* qr */
	ck_null,	/* subst */
	ck_null,	/* substcont */
	ck_null,	/* trans */
	ck_null,	/* sassign */
	ck_null,	/* aassign */
	ck_spair,	/* chop */
	ck_null,	/* schop */
	ck_spair,	/* chomp */
	ck_null,	/* schomp */
	ck_rfun,	/* defined */
	ck_lfun,	/* undef */
	ck_fun,		/* study */
	ck_lfun,	/* pos */
	ck_lfun,	/* preinc */
	ck_lfun,	/* i_preinc */
	ck_lfun,	/* predec */
	ck_lfun,	/* i_predec */
	ck_lfun,	/* postinc */
	ck_lfun,	/* i_postinc */
	ck_lfun,	/* postdec */
	ck_lfun,	/* i_postdec */
	ck_null,	/* pow */
	ck_null,	/* multiply */
	ck_null,	/* i_multiply */
	ck_null,	/* divide */
	ck_null,	/* i_divide */
	ck_null,	/* modulo */
	ck_null,	/* i_modulo */
	ck_repeat,	/* repeat */
	ck_null,	/* add */
	ck_null,	/* i_add */
	ck_null,	/* subtract */
	ck_null,	/* i_subtract */
	ck_concat,	/* concat */
	ck_fun,		/* stringify */
	ck_bitop,	/* left_shift */
	ck_bitop,	/* right_shift */
	ck_null,	/* lt */
	ck_null,	/* i_lt */
	ck_null,	/* gt */
	ck_null,	/* i_gt */
	ck_null,	/* le */
	ck_null,	/* i_le */
	ck_null,	/* ge */
	ck_null,	/* i_ge */
	ck_null,	/* eq */
	ck_null,	/* i_eq */
	ck_null,	/* ne */
	ck_null,	/* i_ne */
	ck_null,	/* ncmp */
	ck_null,	/* i_ncmp */
	ck_scmp,	/* slt */
	ck_scmp,	/* sgt */
	ck_scmp,	/* sle */
	ck_scmp,	/* sge */
	ck_null,	/* seq */
	ck_null,	/* sne */
	ck_scmp,	/* scmp */
	ck_bitop,	/* bit_and */
	ck_bitop,	/* bit_xor */
	ck_bitop,	/* bit_or */
	ck_null,	/* negate */
	ck_null,	/* i_negate */
	ck_null,	/* not */
	ck_bitop,	/* complement */
	ck_fun,		/* atan2 */
	ck_fun,		/* sin */
	ck_fun,		/* cos */
	ck_fun,		/* rand */
	ck_fun,		/* srand */
	ck_fun,		/* exp */
	ck_fun,		/* log */
	ck_fun,		/* sqrt */
	ck_fun,		/* int */
	ck_fun,		/* hex */
	ck_fun,		/* oct */
	ck_fun,		/* abs */
	ck_lengthconst,	/* length */
	ck_fun,		/* substr */
	ck_fun,		/* vec */
	ck_index,	/* index */
	ck_index,	/* rindex */
	ck_fun_locale,	/* sprintf */
	ck_fun,		/* formline */
	ck_fun,		/* ord */
	ck_fun,		/* chr */
	ck_fun,		/* crypt */
	ck_fun_locale,	/* ucfirst */
	ck_fun_locale,	/* lcfirst */
	ck_fun_locale,	/* uc */
	ck_fun_locale,	/* lc */
	ck_fun,		/* quotemeta */
	ck_rvconst,	/* rv2av */
	ck_null,	/* aelemfast */
	ck_null,	/* aelem */
	ck_null,	/* aslice */
	ck_fun,		/* each */
	ck_fun,		/* values */
	ck_fun,		/* keys */
	ck_delete,	/* delete */
	ck_exists,	/* exists */
	ck_rvconst,	/* rv2hv */
	ck_null,	/* helem */
	ck_null,	/* hslice */
	ck_fun,		/* unpack */
	ck_fun,		/* pack */
	ck_split,	/* split */
	ck_fun,		/* join */
	ck_null,	/* list */
	ck_null,	/* lslice */
	ck_fun,		/* anonlist */
	ck_fun,		/* anonhash */
	ck_fun,		/* splice */
	ck_fun,		/* push */
	ck_shift,	/* pop */
	ck_shift,	/* shift */
	ck_fun,		/* unshift */
	ck_sort,	/* sort */
	ck_fun,		/* reverse */
	ck_grep,	/* grepstart */
	ck_null,	/* grepwhile */
	ck_grep,	/* mapstart */
	ck_null,	/* mapwhile */
	ck_null,	/* range */
	ck_null,	/* flip */
	ck_null,	/* flop */
	ck_null,	/* and */
	ck_null,	/* or */
	ck_null,	/* xor */
	ck_null,	/* cond_expr */
	ck_null,	/* andassign */
	ck_null,	/* orassign */
	ck_null,	/* method */
	ck_subr,	/* entersub */
	ck_null,	/* leavesub */
	ck_fun,		/* caller */
	ck_fun,		/* warn */
	ck_fun,		/* die */
	ck_fun,		/* reset */
	ck_null,	/* lineseq */
	ck_null,	/* nextstate */
	ck_null,	/* dbstate */
	ck_null,	/* unstack */
	ck_null,	/* enter */
	ck_null,	/* leave */
	ck_null,	/* scope */
	ck_null,	/* enteriter */
	ck_null,	/* iter */
	ck_null,	/* enterloop */
	ck_null,	/* leaveloop */
	ck_null,	/* return */
	ck_null,	/* last */
	ck_null,	/* next */
	ck_null,	/* redo */
	ck_null,	/* dump */
	ck_null,	/* goto */
	ck_fun,		/* exit */
	ck_fun,		/* open */
	ck_fun,		/* close */
	ck_fun,		/* pipe_op */
	ck_fun,		/* fileno */
	ck_fun,		/* umask */
	ck_fun,		/* binmode */
	ck_fun,		/* tie */
	ck_fun,		/* untie */
	ck_fun,		/* tied */
	ck_fun,		/* dbmopen */
	ck_fun,		/* dbmclose */
	ck_select,	/* sselect */
	ck_select,	/* select */
	ck_eof,		/* getc */
	ck_fun,		/* read */
	ck_fun,		/* enterwrite */
	ck_null,	/* leavewrite */
	ck_listiob,	/* prtf */
	ck_listiob,	/* print */
	ck_fun,		/* sysopen */
	ck_fun,		/* sysseek */
	ck_fun,		/* sysread */
	ck_fun,		/* syswrite */
	ck_fun,		/* send */
	ck_fun,		/* recv */
	ck_eof,		/* eof */
	ck_fun,		/* tell */
	ck_fun,		/* seek */
	ck_trunc,	/* truncate */
	ck_fun,		/* fcntl */
	ck_fun,		/* ioctl */
	ck_fun,		/* flock */
	ck_fun,		/* socket */
	ck_fun,		/* sockpair */
	ck_fun,		/* bind */
	ck_fun,		/* connect */
	ck_fun,		/* listen */
	ck_fun,		/* accept */
	ck_fun,		/* shutdown */
	ck_fun,		/* gsockopt */
	ck_fun,		/* ssockopt */
	ck_fun,		/* getsockname */
	ck_fun,		/* getpeername */
	ck_ftst,	/* lstat */
	ck_ftst,	/* stat */
	ck_ftst,	/* ftrread */
	ck_ftst,	/* ftrwrite */
	ck_ftst,	/* ftrexec */
	ck_ftst,	/* fteread */
	ck_ftst,	/* ftewrite */
	ck_ftst,	/* fteexec */
	ck_ftst,	/* ftis */
	ck_ftst,	/* fteowned */
	ck_ftst,	/* ftrowned */
	ck_ftst,	/* ftzero */
	ck_ftst,	/* ftsize */
	ck_ftst,	/* ftmtime */
	ck_ftst,	/* ftatime */
	ck_ftst,	/* ftctime */
	ck_ftst,	/* ftsock */
	ck_ftst,	/* ftchr */
	ck_ftst,	/* ftblk */
	ck_ftst,	/* ftfile */
	ck_ftst,	/* ftdir */
	ck_ftst,	/* ftpipe */
	ck_ftst,	/* ftlink */
	ck_ftst,	/* ftsuid */
	ck_ftst,	/* ftsgid */
	ck_ftst,	/* ftsvtx */
	ck_ftst,	/* fttty */
	ck_ftst,	/* fttext */
	ck_ftst,	/* ftbinary */
	ck_fun,		/* chdir */
	ck_fun,		/* chown */
	ck_fun,		/* chroot */
	ck_fun,		/* unlink */
	ck_fun,		/* chmod */
	ck_fun,		/* utime */
	ck_fun,		/* rename */
	ck_fun,		/* link */
	ck_fun,		/* symlink */
	ck_fun,		/* readlink */
	ck_fun,		/* mkdir */
	ck_fun,		/* rmdir */
	ck_fun,		/* open_dir */
	ck_fun,		/* readdir */
	ck_fun,		/* telldir */
	ck_fun,		/* seekdir */
	ck_fun,		/* rewinddir */
	ck_fun,		/* closedir */
	ck_null,	/* fork */
	ck_null,	/* wait */
	ck_fun,		/* waitpid */
	ck_exec,	/* system */
	ck_exec,	/* exec */
	ck_fun,		/* kill */
	ck_null,	/* getppid */
	ck_fun,		/* getpgrp */
	ck_fun,		/* setpgrp */
	ck_fun,		/* getpriority */
	ck_fun,		/* setpriority */
	ck_null,	/* time */
	ck_null,	/* tms */
	ck_fun,		/* localtime */
	ck_fun,		/* gmtime */
	ck_fun,		/* alarm */
	ck_fun,		/* sleep */
	ck_fun,		/* shmget */
	ck_fun,		/* shmctl */
	ck_fun,		/* shmread */
	ck_fun,		/* shmwrite */
	ck_fun,		/* msgget */
	ck_fun,		/* msgctl */
	ck_fun,		/* msgsnd */
	ck_fun,		/* msgrcv */
	ck_fun,		/* semget */
	ck_fun,		/* semctl */
	ck_fun,		/* semop */
	ck_require,	/* require */
	ck_fun,		/* dofile */
	ck_eval,	/* entereval */
	ck_null,	/* leaveeval */
	ck_null,	/* entertry */
	ck_null,	/* leavetry */
	ck_fun,		/* ghbyname */
	ck_fun,		/* ghbyaddr */
	ck_null,	/* ghostent */
	ck_fun,		/* gnbyname */
	ck_fun,		/* gnbyaddr */
	ck_null,	/* gnetent */
	ck_fun,		/* gpbyname */
	ck_fun,		/* gpbynumber */
	ck_null,	/* gprotoent */
	ck_fun,		/* gsbyname */
	ck_fun,		/* gsbyport */
	ck_null,	/* gservent */
	ck_fun,		/* shostent */
	ck_fun,		/* snetent */
	ck_fun,		/* sprotoent */
	ck_fun,		/* sservent */
	ck_null,	/* ehostent */
	ck_null,	/* enetent */
	ck_null,	/* eprotoent */
	ck_null,	/* eservent */
	ck_fun,		/* gpwnam */
	ck_fun,		/* gpwuid */
	ck_null,	/* gpwent */
	ck_null,	/* spwent */
	ck_null,	/* epwent */
	ck_fun,		/* ggrnam */
	ck_fun,		/* ggrgid */
	ck_null,	/* ggrent */
	ck_null,	/* sgrent */
	ck_null,	/* egrent */
	ck_null,	/* getlogin */
	ck_fun,		/* syscall */
	ck_rfun,	/* lock */
	ck_null,	/* threadsv */
};

OP * (CPERLscope(*ppaddr)[])(ARGSproto) = {
	pp_null,
	pp_stub,
	pp_scalar,
	pp_pushmark,
	pp_wantarray,
	pp_const,
	pp_gvsv,
	pp_gv,
	pp_gelem,
	pp_padsv,
	pp_padav,
	pp_padhv,
	pp_padany,
	pp_pushre,
	pp_rv2gv,
	pp_rv2sv,
	pp_av2arylen,
	pp_rv2cv,
	pp_anoncode,
	pp_prototype,
	pp_refgen,
	pp_srefgen,
	pp_ref,
	pp_bless,
	pp_backtick,
	pp_glob,
	pp_readline,
	pp_rcatline,
	pp_regcmaybe,
	pp_regcreset,
	pp_regcomp,
	pp_match,
	pp_qr,
	pp_subst,
	pp_substcont,
	pp_trans,
	pp_sassign,
	pp_aassign,
	pp_chop,
	pp_schop,
	pp_chomp,
	pp_schomp,
	pp_defined,
	pp_undef,
	pp_study,
	pp_pos,
	pp_preinc,
	pp_i_preinc,
	pp_predec,
	pp_i_predec,
	pp_postinc,
	pp_i_postinc,
	pp_postdec,
	pp_i_postdec,
	pp_pow,
	pp_multiply,
	pp_i_multiply,
	pp_divide,
	pp_i_divide,
	pp_modulo,
	pp_i_modulo,
	pp_repeat,
	pp_add,
	pp_i_add,
	pp_subtract,
	pp_i_subtract,
	pp_concat,
	pp_stringify,
	pp_left_shift,
	pp_right_shift,
	pp_lt,
	pp_i_lt,
	pp_gt,
	pp_i_gt,
	pp_le,
	pp_i_le,
	pp_ge,
	pp_i_ge,
	pp_eq,
	pp_i_eq,
	pp_ne,
	pp_i_ne,
	pp_ncmp,
	pp_i_ncmp,
	pp_slt,
	pp_sgt,
	pp_sle,
	pp_sge,
	pp_seq,
	pp_sne,
	pp_scmp,
	pp_bit_and,
	pp_bit_xor,
	pp_bit_or,
	pp_negate,
	pp_i_negate,
	pp_not,
	pp_complement,
	pp_atan2,
	pp_sin,
	pp_cos,
	pp_rand,
	pp_srand,
	pp_exp,
	pp_log,
	pp_sqrt,
	pp_int,
	pp_hex,
	pp_oct,
	pp_abs,
	pp_length,
	pp_substr,
	pp_vec,
	pp_index,
	pp_rindex,
	pp_sprintf,
	pp_formline,
	pp_ord,
	pp_chr,
	pp_crypt,
	pp_ucfirst,
	pp_lcfirst,
	pp_uc,
	pp_lc,
	pp_quotemeta,
	pp_rv2av,
	pp_aelemfast,
	pp_aelem,
	pp_aslice,
	pp_each,
	pp_values,
	pp_keys,
	pp_delete,
	pp_exists,
	pp_rv2hv,
	pp_helem,
	pp_hslice,
	pp_unpack,
	pp_pack,
	pp_split,
	pp_join,
	pp_list,
	pp_lslice,
	pp_anonlist,
	pp_anonhash,
	pp_splice,
	pp_push,
	pp_pop,
	pp_shift,
	pp_unshift,
	pp_sort,
	pp_reverse,
	pp_grepstart,
	pp_grepwhile,
	pp_mapstart,
	pp_mapwhile,
	pp_range,
	pp_flip,
	pp_flop,
	pp_and,
	pp_or,
	pp_xor,
	pp_cond_expr,
	pp_andassign,
	pp_orassign,
	pp_method,
	pp_entersub,
	pp_leavesub,
	pp_caller,
	pp_warn,
	pp_die,
	pp_reset,
	pp_lineseq,
	pp_nextstate,
	pp_dbstate,
	pp_unstack,
	pp_enter,
	pp_leave,
	pp_scope,
	pp_enteriter,
	pp_iter,
	pp_enterloop,
	pp_leaveloop,
	pp_return,
	pp_last,
	pp_next,
	pp_redo,
	pp_dump,
	pp_goto,
	pp_exit,
	pp_open,
	pp_close,
	pp_pipe_op,
	pp_fileno,
	pp_umask,
	pp_binmode,
	pp_tie,
	pp_untie,
	pp_tied,
	pp_dbmopen,
	pp_dbmclose,
	pp_sselect,
	pp_select,
	pp_getc,
	pp_read,
	pp_enterwrite,
	pp_leavewrite,
	pp_prtf,
	pp_print,
	pp_sysopen,
	pp_sysseek,
	pp_sysread,
	pp_syswrite,
	pp_send,
	pp_recv,
	pp_eof,
	pp_tell,
	pp_seek,
	pp_truncate,
	pp_fcntl,
	pp_ioctl,
	pp_flock,
	pp_socket,
	pp_sockpair,
	pp_bind,
	pp_connect,
	pp_listen,
	pp_accept,
	pp_shutdown,
	pp_gsockopt,
	pp_ssockopt,
	pp_getsockname,
	pp_getpeername,
	pp_lstat,
	pp_stat,
	pp_ftrread,
	pp_ftrwrite,
	pp_ftrexec,
	pp_fteread,
	pp_ftewrite,
	pp_fteexec,
	pp_ftis,
	pp_fteowned,
	pp_ftrowned,
	pp_ftzero,
	pp_ftsize,
	pp_ftmtime,
	pp_ftatime,
	pp_ftctime,
	pp_ftsock,
	pp_ftchr,
	pp_ftblk,
	pp_ftfile,
	pp_ftdir,
	pp_ftpipe,
	pp_ftlink,
	pp_ftsuid,
	pp_ftsgid,
	pp_ftsvtx,
	pp_fttty,
	pp_fttext,
	pp_ftbinary,
	pp_chdir,
	pp_chown,
	pp_chroot,
	pp_unlink,
	pp_chmod,
	pp_utime,
	pp_rename,
	pp_link,
	pp_symlink,
	pp_readlink,
	pp_mkdir,
	pp_rmdir,
	pp_open_dir,
	pp_readdir,
	pp_telldir,
	pp_seekdir,
	pp_rewinddir,
	pp_closedir,
	pp_fork,
	pp_wait,
	pp_waitpid,
	pp_system,
	pp_exec,
	pp_kill,
	pp_getppid,
	pp_getpgrp,
	pp_setpgrp,
	pp_getpriority,
	pp_setpriority,
	pp_time,
	pp_tms,
	pp_localtime,
	pp_gmtime,
	pp_alarm,
	pp_sleep,
	pp_shmget,
	pp_shmctl,
	pp_shmread,
	pp_shmwrite,
	pp_msgget,
	pp_msgctl,
	pp_msgsnd,
	pp_msgrcv,
	pp_semget,
	pp_semctl,
	pp_semop,
	pp_require,
	pp_dofile,
	pp_entereval,
	pp_leaveeval,
	pp_entertry,
	pp_leavetry,
	pp_ghbyname,
	pp_ghbyaddr,
	pp_ghostent,
	pp_gnbyname,
	pp_gnbyaddr,
	pp_gnetent,
	pp_gpbyname,
	pp_gpbynumber,
	pp_gprotoent,
	pp_gsbyname,
	pp_gsbyport,
	pp_gservent,
	pp_shostent,
	pp_snetent,
	pp_sprotoent,
	pp_sservent,
	pp_ehostent,
	pp_enetent,
	pp_eprotoent,
	pp_eservent,
	pp_gpwnam,
	pp_gpwuid,
	pp_gpwent,
	pp_spwent,
	pp_epwent,
	pp_ggrnam,
	pp_ggrgid,
	pp_ggrent,
	pp_sgrent,
	pp_egrent,
	pp_getlogin,
	pp_syscall,
	pp_lock,
	pp_threadsv,
};

int
fprintf(PerlIO *stream, const char *format, ...)
{
    va_list(arglist);
    va_start(arglist, format);
    return PerlIO_vprintf(stream, format, arglist);
}

#undef PERLVAR
#define PERLVAR(x, y)
#undef PERLVARI
#define PERLVARI(x, y, z) PL_##x = z;
#undef PERLVARIC
#define PERLVARIC(x, y, z) PL_##x = z;

CPerlObj::CPerlObj(IPerlMem* ipM, IPerlEnv* ipE, IPerlStdIO* ipStd,
					     IPerlLIO* ipLIO, IPerlDir* ipD, IPerlSock* ipS, IPerlProc* ipP)
{
    memset(((char*)this)+sizeof(void*), 0, sizeof(CPerlObj)-sizeof(void*));

#include "thrdvar.h"
#include "intrpvar.h"
#include "perlvars.h"

    PL_piMem = ipM;
    PL_piENV = ipE;
    PL_piStdIO = ipStd;
    PL_piLIO = ipLIO;
    PL_piDir = ipD;
    PL_piSock = ipS;
    PL_piProc = ipP;
}

void*
CPerlObj::operator new(size_t nSize, IPerlMem *pvtbl)
{
    if(pvtbl != NULL)
	return pvtbl->Malloc(nSize);

    return NULL;
}

int&
CPerlObj::ErrorNo(void)
{
    return errno;
}

void
CPerlObj::Init(void)
{
}

#ifdef WIN32		/* XXX why are these needed? */
bool
do_exec(char *cmd)
{
    return PerlProc_Cmd(cmd);
}

int
do_aspawn(void *vreally, void **vmark, void **vsp)
{
    return PerlProc_aspawn(vreally, vmark, vsp);
}
#endif  /* WIN32 */

#endif   /* PERL_OBJECT */
