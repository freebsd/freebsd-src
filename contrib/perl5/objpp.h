#ifndef __Objpp_h__
#define __Objpp_h__

#undef  amagic_call
#define amagic_call       CPerlObj::Perl_amagic_call
#undef  amagic_cmp
#define amagic_cmp        CPerlObj::amagic_cmp
#undef  amagic_cmp_locale
#define amagic_cmp_locale CPerlObj::amagic_cmp_locale
#undef  Gv_AMupdate
#define Gv_AMupdate       CPerlObj::Perl_Gv_AMupdate
#undef  add_data
#define add_data          CPerlObj::add_data
#undef  ao
#define ao                CPerlObj::ao
#undef  append_elem
#define append_elem       CPerlObj::Perl_append_elem
#undef  append_list
#define append_list       CPerlObj::Perl_append_list
#undef  apply
#define apply             CPerlObj::Perl_apply
#undef  asIV
#define asIV              CPerlObj::asIV
#undef  asUV
#define asUV              CPerlObj::asUV
#undef  assertref
#define assertref         CPerlObj::Perl_assertref
#undef  av_clear
#define av_clear          CPerlObj::Perl_av_clear
#undef  av_extend
#define av_extend         CPerlObj::Perl_av_extend
#undef  av_fake
#define av_fake           CPerlObj::Perl_av_fake
#undef  av_fetch
#define av_fetch          CPerlObj::Perl_av_fetch
#undef  av_fill
#define av_fill           CPerlObj::Perl_av_fill
#undef  av_len
#define av_len            CPerlObj::Perl_av_len
#undef  av_make
#define av_make           CPerlObj::Perl_av_make
#undef  av_pop
#define av_pop            CPerlObj::Perl_av_pop
#undef  av_push
#define av_push           CPerlObj::Perl_av_push
#undef  av_shift
#define av_shift          CPerlObj::Perl_av_shift
#undef  av_reify
#define av_reify          CPerlObj::Perl_av_reify
#undef  av_store
#define av_store          CPerlObj::Perl_av_store
#undef  av_undef 
#define av_undef          CPerlObj::Perl_av_undef 
#undef  av_unshift
#define av_unshift        CPerlObj::Perl_av_unshift
#undef  avhv_keys
#define avhv_keys         CPerlObj::Perl_avhv_keys
#undef  avhv_fetch_ent
#define avhv_fetch_ent    CPerlObj::Perl_avhv_fetch_ent
#undef  avhv_exists_ent
#define avhv_exists_ent   CPerlObj::Perl_avhv_exists_ent
#undef  avhv_index_sv
#define avhv_index_sv     CPerlObj::avhv_index_sv
#undef  avhv_iternext
#define avhv_iternext     CPerlObj::Perl_avhv_iternext
#undef  avhv_iterval
#define avhv_iterval      CPerlObj::Perl_avhv_iterval
#undef  bad_type
#define bad_type          CPerlObj::bad_type
#undef  bind_match
#define bind_match        CPerlObj::Perl_bind_match
#undef  block_end
#define block_end         CPerlObj::Perl_block_end
#undef  block_gimme
#define block_gimme       CPerlObj::Perl_block_gimme
#undef  block_start
#define block_start       CPerlObj::Perl_block_start
#undef  bset_obj_store
#define bset_obj_store    CPerlObj::Perl_bset_obj_store
#undef  byterun
#define byterun           CPerlObj::Perl_byterun
#undef  call_list
#define call_list         CPerlObj::Perl_call_list
#undef  cando
#define cando             CPerlObj::Perl_cando
#undef  cast_ulong
#define cast_ulong        CPerlObj::cast_ulong
#undef  checkcomma
#define checkcomma        CPerlObj::Perl_checkcomma
#undef  check_uni
#define check_uni         CPerlObj::Perl_check_uni
#undef  ck_anoncode
#define ck_anoncode       CPerlObj::Perl_ck_anoncode
#undef  ck_bitop
#define ck_bitop          CPerlObj::Perl_ck_bitop
#undef  ck_concat
#define ck_concat         CPerlObj::Perl_ck_concat
#undef  ck_delete
#define ck_delete         CPerlObj::Perl_ck_delete
#undef  ck_eof
#define ck_eof            CPerlObj::Perl_ck_eof
#undef  ck_eval
#define ck_eval           CPerlObj::Perl_ck_eval
#undef  ck_exec
#define ck_exec           CPerlObj::Perl_ck_exec
#undef  ck_exists
#define ck_exists         CPerlObj::Perl_ck_exists
#undef  ck_formline
#define ck_formline       CPerlObj::Perl_ck_formline
#undef  ck_ftst
#define ck_ftst           CPerlObj::Perl_ck_ftst
#undef  ck_fun
#define ck_fun            CPerlObj::Perl_ck_fun
#undef  ck_fun_locale
#define ck_fun_locale     CPerlObj::Perl_ck_fun_locale
#undef  ck_glob
#define ck_glob           CPerlObj::Perl_ck_glob
#undef  ck_grep
#define ck_grep           CPerlObj::Perl_ck_grep
#undef  ck_gvconst
#define ck_gvconst        CPerlObj::Perl_ck_gvconst
#undef  ck_index
#define ck_index          CPerlObj::Perl_ck_index
#undef  ck_lengthconst
#define ck_lengthconst    CPerlObj::Perl_ck_lengthconst
#undef  ck_lfun
#define ck_lfun           CPerlObj::Perl_ck_lfun
#undef  ck_listiob
#define ck_listiob        CPerlObj::Perl_ck_listiob
#undef  ck_match
#define ck_match          CPerlObj::Perl_ck_match
#undef  ck_null
#define ck_null           CPerlObj::Perl_ck_null
#undef  ck_repeat
#define ck_repeat         CPerlObj::Perl_ck_repeat
#undef  ck_require
#define ck_require        CPerlObj::Perl_ck_require
#undef  ck_retarget
#define ck_retarget       CPerlObj::Perl_ck_retarget
#undef  ck_rfun
#define ck_rfun           CPerlObj::Perl_ck_rfun
#undef  ck_rvconst
#define ck_rvconst        CPerlObj::Perl_ck_rvconst
#undef  ck_scmp
#define ck_scmp           CPerlObj::Perl_ck_scmp
#undef  ck_select
#define ck_select         CPerlObj::Perl_ck_select
#undef  ck_shift
#define ck_shift          CPerlObj::Perl_ck_shift
#undef  ck_sort
#define ck_sort           CPerlObj::Perl_ck_sort
#undef  ck_spair
#define ck_spair          CPerlObj::Perl_ck_spair
#undef  ck_split
#define ck_split          CPerlObj::Perl_ck_split
#undef  ck_subr
#define ck_subr           CPerlObj::Perl_ck_subr
#undef  ck_svconst
#define ck_svconst        CPerlObj::Perl_ck_svconst
#undef  ck_trunc
#define ck_trunc          CPerlObj::Perl_ck_trunc
#undef  convert
#define convert           CPerlObj::Perl_convert
#undef  cpytill
#define cpytill           CPerlObj::Perl_cpytill
#undef  croak
#define croak             CPerlObj::Perl_croak
#undef  cv_ckproto
#define cv_ckproto        CPerlObj::Perl_cv_ckproto
#undef  cv_clone
#define cv_clone          CPerlObj::Perl_cv_clone
#undef  cv_clone2
#define cv_clone2         CPerlObj::cv_clone2
#undef  cv_const_sv
#define cv_const_sv       CPerlObj::Perl_cv_const_sv
#undef  cv_undef 
#define cv_undef          CPerlObj::Perl_cv_undef 
#undef  cx_dump
#define cx_dump           CPerlObj::Perl_cx_dump
#undef  cxinc
#define cxinc             CPerlObj::Perl_cxinc
#undef  deb
#define deb               CPerlObj::Perl_deb
#undef  deb_growlevel
#define deb_growlevel     CPerlObj::Perl_deb_growlevel
#undef  debop
#define debop             CPerlObj::Perl_debop
#undef  debstackptrs
#define debstackptrs      CPerlObj::Perl_debstackptrs
#undef  debprof
#define debprof           CPerlObj::debprof
#undef  debprofdump
#define debprofdump       CPerlObj::Perl_debprofdump
#undef  debstack
#define debstack          CPerlObj::Perl_debstack
#undef  del_sv
#define del_sv            CPerlObj::del_sv
#undef  del_xiv
#define del_xiv           CPerlObj::del_xiv
#undef  del_xnv
#define del_xnv           CPerlObj::del_xnv
#undef  del_xpv
#define del_xpv           CPerlObj::del_xpv
#undef  del_xrv
#define del_xrv           CPerlObj::del_xrv
#undef  delimcpy
#define delimcpy          CPerlObj::Perl_delimcpy
#undef  depcom
#define depcom            CPerlObj::depcom
#undef  deprecate
#define deprecate         CPerlObj::Perl_deprecate
#undef  die
#define die               CPerlObj::Perl_die
#undef  die_where
#define die_where         CPerlObj::Perl_die_where
#undef  div128
#define div128            CPerlObj::div128
#undef  doencodes
#define doencodes         CPerlObj::doencodes
#undef  doeval
#define doeval            CPerlObj::doeval
#undef  doform
#define doform            CPerlObj::doform
#undef  dofindlabel
#define dofindlabel       CPerlObj::Perl_dofindlabel
#undef  doparseform
#define doparseform       CPerlObj::doparseform
#undef  dopoptoeval
#define dopoptoeval       CPerlObj::Perl_dopoptoeval
#undef  dopoptolabel
#define dopoptolabel      CPerlObj::dopoptolabel
#undef  dopoptoloop
#define dopoptoloop       CPerlObj::dopoptoloop
#undef  dopoptosub
#define dopoptosub        CPerlObj::dopoptosub
#undef  dopoptosub_at
#define dopoptosub_at     CPerlObj::dopoptosub_at
#undef  dounwind
#define dounwind          CPerlObj::Perl_dounwind
#undef  do_aexec
#define do_aexec          CPerlObj::Perl_do_aexec
#undef  do_aspawn
#define do_aspawn         CPerlObj::do_aspawn
#undef  do_binmode
#define do_binmode        CPerlObj::Perl_do_binmode
#undef  do_chop
#define do_chop           CPerlObj::Perl_do_chop
#undef  do_close
#define do_close          CPerlObj::Perl_do_close
#undef  do_eof
#define do_eof            CPerlObj::Perl_do_eof
#undef  do_exec
#define do_exec           CPerlObj::Perl_do_exec
#undef  do_execfree
#define do_execfree       CPerlObj::Perl_do_execfree
#undef  do_ipcctl
#define do_ipcctl         CPerlObj::Perl_do_ipcctl
#undef  do_ipcget
#define do_ipcget         CPerlObj::Perl_do_ipcget
#undef  do_join
#define do_join           CPerlObj::Perl_do_join
#undef  do_kv
#define do_kv             CPerlObj::Perl_do_kv
#undef  do_msgrcv
#define do_msgrcv         CPerlObj::Perl_do_msgrcv
#undef  do_msgsnd
#define do_msgsnd         CPerlObj::Perl_do_msgsnd
#undef  do_open
#define do_open           CPerlObj::Perl_do_open
#undef  do_pipe
#define do_pipe           CPerlObj::Perl_do_pipe
#undef  do_print
#define do_print          CPerlObj::Perl_do_print
#undef  do_readline
#define do_readline       CPerlObj::Perl_do_readline
#undef  do_chomp
#define do_chomp          CPerlObj::Perl_do_chomp
#undef  do_seek
#define do_seek           CPerlObj::Perl_do_seek
#undef  do_semop
#define do_semop          CPerlObj::Perl_do_semop
#undef  do_shmio
#define do_shmio          CPerlObj::Perl_do_shmio
#undef  do_sprintf
#define do_sprintf        CPerlObj::Perl_do_sprintf
#undef  do_sysseek
#define do_sysseek        CPerlObj::Perl_do_sysseek
#undef  do_tell
#define do_tell           CPerlObj::Perl_do_tell
#undef  do_trans
#define do_trans          CPerlObj::Perl_do_trans
#undef  do_vecset
#define do_vecset         CPerlObj::Perl_do_vecset
#undef  do_vop
#define do_vop            CPerlObj::Perl_do_vop
#undef  dofile
#define dofile            CPerlObj::Perl_dofile
#undef  do_clean_all
#define do_clean_all      CPerlObj::do_clean_all
#undef  do_clean_named_objs
#define do_clean_named_objs   CPerlObj::do_clean_named_objs
#undef  do_clean_objs
#define do_clean_objs     CPerlObj::do_clean_objs
#undef  do_report_used
#define do_report_used    CPerlObj::do_report_used
#undef  docatch
#define docatch           CPerlObj::docatch
#undef  dowantarray
#define dowantarray       CPerlObj::Perl_dowantarray
#undef  dump
#define dump              CPerlObj::dump
#undef  dump_all
#define dump_all          CPerlObj::Perl_dump_all
#undef  dump_eval
#define dump_eval         CPerlObj::Perl_dump_eval
#undef  dump_fds
#define dump_fds          CPerlObj::Perl_dump_fds
#undef  dump_form
#define dump_form         CPerlObj::Perl_dump_form
#undef  dump_gv
#define dump_gv           CPerlObj::Perl_dump_gv
#undef  dump_mstats
#define dump_mstats       CPerlObj::Perl_dump_mstats
#undef  dump_op
#define dump_op           CPerlObj::Perl_dump_op
#undef  dump_pm
#define dump_pm           CPerlObj::Perl_dump_pm
#undef  dump_packsubs
#define dump_packsubs     CPerlObj::Perl_dump_packsubs
#undef  dump_sub
#define dump_sub          CPerlObj::Perl_dump_sub
#undef  dumpuntil
#define dumpuntil         CPerlObj::dumpuntil
#undef  fbm_compile
#define fbm_compile       CPerlObj::Perl_fbm_compile
#undef  fbm_instr
#define fbm_instr         CPerlObj::Perl_fbm_instr
#undef  filter_add
#define filter_add        CPerlObj::Perl_filter_add
#undef  filter_del
#define filter_del        CPerlObj::Perl_filter_del
#undef  filter_gets
#define filter_gets       CPerlObj::filter_gets
#undef  filter_read
#define filter_read       CPerlObj::Perl_filter_read
#undef  find_beginning
#define find_beginning    CPerlObj::find_beginning
#undef  find_script
#define find_script       CPerlObj::Perl_find_script
#undef  forbid_setid
#define forbid_setid      CPerlObj::forbid_setid
#undef  force_ident
#define force_ident       CPerlObj::Perl_force_ident
#undef  force_list
#define force_list        CPerlObj::Perl_force_list
#undef  force_next
#define force_next        CPerlObj::Perl_force_next
#undef  force_word
#define force_word        CPerlObj::Perl_force_word
#undef  force_version
#define force_version     CPerlObj::force_version
#undef  form
#define form              CPerlObj::Perl_form
#undef  fold_constants
#define fold_constants    CPerlObj::Perl_fold_constants
#undef  fprintf
#define fprintf           CPerlObj::fprintf
#undef  free_tmps
#define free_tmps         CPerlObj::Perl_free_tmps
#undef  gen_constant_list
#define gen_constant_list CPerlObj::Perl_gen_constant_list
#undef  get_db_sub
#define get_db_sub        CPerlObj::get_db_sub
#undef  get_op_descs
#define get_op_descs      CPerlObj::Perl_get_op_descs
#undef  get_op_names
#define get_op_names      CPerlObj::Perl_get_op_names
#undef  get_no_modify
#define get_no_modify     CPerlObj::Perl_get_no_modify
#undef  get_opargs
#define get_opargs        CPerlObj::Perl_get_opargs
#undef  get_specialsv_list
#define get_specialsv_list CPerlObj::Perl_get_specialsv_list
#undef  get_vtbl
#define get_vtbl          CPerlObj::Perl_get_vtbl
#undef  getlogin
#define getlogin          CPerlObj::getlogin
#undef  gp_free
#define gp_free           CPerlObj::Perl_gp_free
#undef  gp_ref
#define gp_ref            CPerlObj::Perl_gp_ref
#undef  gv_autoload4
#define gv_autoload4      CPerlObj::Perl_gv_autoload4
#undef  gv_AVadd
#define gv_AVadd          CPerlObj::Perl_gv_AVadd
#undef  gv_HVadd
#define gv_HVadd          CPerlObj::Perl_gv_HVadd
#undef  gv_IOadd
#define gv_IOadd          CPerlObj::Perl_gv_IOadd
#undef  gv_check
#define gv_check          CPerlObj::Perl_gv_check
#undef  gv_efullname
#define gv_efullname      CPerlObj::Perl_gv_efullname
#undef  gv_efullname3
#define gv_efullname3     CPerlObj::Perl_gv_efullname3
#undef  gv_ename
#define gv_ename          CPerlObj::gv_ename
#undef  gv_fetchfile
#define gv_fetchfile      CPerlObj::Perl_gv_fetchfile
#undef  gv_fetchmeth
#define gv_fetchmeth      CPerlObj::Perl_gv_fetchmeth
#undef  gv_fetchmethod
#define gv_fetchmethod    CPerlObj::Perl_gv_fetchmethod
#undef  gv_fetchmethod_autoload
#define gv_fetchmethod_autoload  CPerlObj::Perl_gv_fetchmethod_autoload
#undef  gv_fetchpv
#define gv_fetchpv        CPerlObj::Perl_gv_fetchpv
#undef  gv_fullname
#define gv_fullname       CPerlObj::Perl_gv_fullname
#undef  gv_fullname3
#define gv_fullname3      CPerlObj::Perl_gv_fullname3
#undef  gv_init
#define gv_init           CPerlObj::Perl_gv_init
#undef  gv_init_sv
#define gv_init_sv        CPerlObj::gv_init_sv
#undef  gv_stashpv
#define gv_stashpv        CPerlObj::Perl_gv_stashpv
#undef  gv_stashpvn
#define gv_stashpvn       CPerlObj::Perl_gv_stashpvn
#undef  gv_stashsv
#define gv_stashsv        CPerlObj::Perl_gv_stashsv
#undef  he_delayfree
#define he_delayfree      CPerlObj::Perl_he_delayfree
#undef  he_free
#define he_free           CPerlObj::Perl_he_free
#undef  hfreeentries
#define hfreeentries      CPerlObj::hfreeentries
#undef  hoistmust
#define hoistmust         CPerlObj::Perl_hoistmust
#undef  hsplit
#define hsplit            CPerlObj::hsplit
#undef  hv_clear
#define hv_clear          CPerlObj::Perl_hv_clear
#undef  hv_delayfree_ent
#define hv_delayfree_ent  CPerlObj::Perl_hv_delayfree_ent
#undef  hv_delete
#define hv_delete         CPerlObj::Perl_hv_delete
#undef  hv_delete_ent
#define hv_delete_ent     CPerlObj::Perl_hv_delete_ent
#undef  hv_exists
#define hv_exists         CPerlObj::Perl_hv_exists
#undef  hv_exists_ent
#define hv_exists_ent     CPerlObj::Perl_hv_exists_ent
#undef  hv_free_ent
#define hv_free_ent       CPerlObj::Perl_hv_free_ent
#undef  hv_fetch
#define hv_fetch          CPerlObj::Perl_hv_fetch
#undef  hv_fetch_ent
#define hv_fetch_ent      CPerlObj::Perl_hv_fetch_ent
#undef  hv_iterinit
#define hv_iterinit       CPerlObj::Perl_hv_iterinit
#undef  hv_iterkey
#define hv_iterkey        CPerlObj::Perl_hv_iterkey
#undef  hv_iterkeysv
#define hv_iterkeysv      CPerlObj::Perl_hv_iterkeysv
#undef  hv_iternext
#define hv_iternext       CPerlObj::Perl_hv_iternext
#undef  hv_iternextsv
#define hv_iternextsv     CPerlObj::Perl_hv_iternextsv
#undef  hv_iterval
#define hv_iterval        CPerlObj::Perl_hv_iterval
#undef  hv_ksplit
#define hv_ksplit         CPerlObj::Perl_hv_ksplit
#undef  hv_magic
#define hv_magic          CPerlObj::Perl_hv_magic
#undef  hv_store
#define hv_store          CPerlObj::Perl_hv_store
#undef  hv_store_ent
#define hv_store_ent      CPerlObj::Perl_hv_store_ent
#undef  hv_undef 
#define hv_undef          CPerlObj::Perl_hv_undef 
#undef  ibcmp
#define ibcmp             CPerlObj::Perl_ibcmp
#undef  ibcmp_locale
#define ibcmp_locale      CPerlObj::Perl_ibcmp_locale
#undef  incpush
#define incpush           CPerlObj::incpush
#undef  incline
#define incline           CPerlObj::incline
#undef  incl_perldb
#define incl_perldb       CPerlObj::incl_perldb
#undef  ingroup
#define ingroup           CPerlObj::Perl_ingroup
#undef  init_debugger
#define init_debugger     CPerlObj::init_debugger
#undef  init_ids
#define init_ids          CPerlObj::init_ids
#undef  init_interp
#define init_interp       CPerlObj::init_interp
#undef  init_main_thread
#define init_main_thread  CPerlObj::init_main_thread
#undef  init_main_stash
#define init_main_stash   CPerlObj::init_main_stash
#undef  init_lexer
#define init_lexer        CPerlObj::init_lexer
#undef  init_perllib
#define init_perllib      CPerlObj::init_perllib
#undef  init_predump_symbols
#define init_predump_symbols  CPerlObj::init_predump_symbols
#undef  init_postdump_symbols
#define init_postdump_symbols  CPerlObj::init_postdump_symbols
#undef  init_stacks
#define init_stacks       CPerlObj::Perl_init_stacks
#undef  intro_my
#define intro_my          CPerlObj::Perl_intro_my
#undef  nuke_stacks
#define nuke_stacks       CPerlObj::nuke_stacks
#undef  instr
#define instr             CPerlObj::Perl_instr
#undef  intuit_method
#define intuit_method     CPerlObj::intuit_method
#undef  intuit_more
#define intuit_more       CPerlObj::Perl_intuit_more
#undef  invert
#define invert            CPerlObj::Perl_invert
#undef  io_close
#define io_close          CPerlObj::Perl_io_close
#undef  is_an_int
#define is_an_int         CPerlObj::is_an_int
#undef  isa_lookup
#define isa_lookup        CPerlObj::isa_lookup
#undef  jmaybe
#define jmaybe            CPerlObj::Perl_jmaybe
#undef  keyword
#define keyword           CPerlObj::Perl_keyword
#undef  leave_scope
#define leave_scope       CPerlObj::Perl_leave_scope
#undef  lex_end
#define lex_end           CPerlObj::Perl_lex_end
#undef  lex_start
#define lex_start         CPerlObj::Perl_lex_start
#undef  linklist
#define linklist          CPerlObj::Perl_linklist
#undef  list
#define list              CPerlObj::Perl_list
#undef  list_assignment
#define list_assignment   CPerlObj::list_assignment
#undef  listkids
#define listkids          CPerlObj::Perl_listkids
#undef  lop
#define lop               CPerlObj::lop
#undef  localize
#define localize          CPerlObj::Perl_localize
#undef  looks_like_number
#define looks_like_number CPerlObj::Perl_looks_like_number
#undef  magic_clearenv
#define magic_clearenv    CPerlObj::Perl_magic_clearenv
#undef  magic_clear_all_env
#define magic_clear_all_env CPerlObj::Perl_magic_clear_all_env
#undef  magic_clearpack
#define magic_clearpack   CPerlObj::Perl_magic_clearpack
#undef  magic_clearsig
#define magic_clearsig    CPerlObj::Perl_magic_clearsig
#undef  magic_existspack
#define magic_existspack  CPerlObj::Perl_magic_existspack
#undef  magic_freeregexp
#define magic_freeregexp  CPerlObj::Perl_magic_freeregexp
#undef  magic_get
#define magic_get         CPerlObj::Perl_magic_get
#undef  magic_getarylen
#define magic_getarylen   CPerlObj::Perl_magic_getarylen
#undef  magic_getdefelem
#define magic_getdefelem  CPerlObj::Perl_magic_getdefelem
#undef  magic_getpack
#define magic_getpack     CPerlObj::Perl_magic_getpack
#undef  magic_getglob
#define magic_getglob     CPerlObj::Perl_magic_getglob
#undef  magic_getnkeys
#define magic_getnkeys    CPerlObj::Perl_magic_getnkeys
#undef  magic_getpos
#define magic_getpos      CPerlObj::Perl_magic_getpos
#undef  magic_getsig
#define magic_getsig      CPerlObj::Perl_magic_getsig
#undef  magic_getsubstr
#define magic_getsubstr   CPerlObj::Perl_magic_getsubstr
#undef  magic_gettaint
#define magic_gettaint    CPerlObj::Perl_magic_gettaint
#undef  magic_getuvar
#define magic_getuvar     CPerlObj::Perl_magic_getuvar
#undef  magic_getvec
#define magic_getvec     CPerlObj::Perl_magic_getvec
#undef  magic_len
#define magic_len         CPerlObj::Perl_magic_len
#undef  magic_methcall
#define magic_methcall    CPerlObj::magic_methcall
#undef  magic_methpack
#define magic_methpack    CPerlObj::magic_methpack
#undef  magic_nextpack
#define magic_nextpack    CPerlObj::Perl_magic_nextpack
#undef  magic_set
#define magic_set         CPerlObj::Perl_magic_set
#undef  magic_set_all_env
#define magic_set_all_env CPerlObj::Perl_magic_set_all_env
#undef  magic_setamagic
#define magic_setamagic   CPerlObj::Perl_magic_setamagic
#undef  magic_setarylen
#define magic_setarylen   CPerlObj::Perl_magic_setarylen
#undef  magic_setbm
#define magic_setbm       CPerlObj::Perl_magic_setbm
#undef  magic_setcollxfrm
#define magic_setcollxfrm CPerlObj::Perl_magic_setcollxfrm
#undef  magic_setdbline
#define magic_setdbline   CPerlObj::Perl_magic_setdbline
#undef  magic_setdefelem
#define magic_setdefelem  CPerlObj::Perl_magic_setdefelem
#undef  magic_setenv
#define magic_setenv      CPerlObj::Perl_magic_setenv
#undef  magic_setfm
#define magic_setfm       CPerlObj::Perl_magic_setfm
#undef  magic_setisa
#define magic_setisa      CPerlObj::Perl_magic_setisa
#undef  magic_setglob
#define magic_setglob     CPerlObj::Perl_magic_setglob
#undef  magic_setmglob
#define magic_setmglob    CPerlObj::Perl_magic_setmglob
#undef  magic_setnkeys
#define magic_setnkeys    CPerlObj::Perl_magic_setnkeys
#undef  magic_setpack
#define magic_setpack     CPerlObj::Perl_magic_setpack
#undef  magic_setpos
#define magic_setpos      CPerlObj::Perl_magic_setpos
#undef  magic_setsig
#define magic_setsig      CPerlObj::Perl_magic_setsig
#undef  magic_setsubstr
#define magic_setsubstr   CPerlObj::Perl_magic_setsubstr
#undef  magic_settaint
#define magic_settaint    CPerlObj::Perl_magic_settaint
#undef  magic_setuvar
#define magic_setuvar     CPerlObj::Perl_magic_setuvar
#undef  magic_setvec
#define magic_setvec      CPerlObj::Perl_magic_setvec
#undef  magic_sizepack
#define magic_sizepack    CPerlObj::Perl_magic_sizepack
#undef  magic_unchain
#define magic_unchain     CPerlObj::Perl_magic_unchain
#undef  magic_wipepack
#define magic_wipepack    CPerlObj::Perl_magic_wipepack
#undef  magicname
#define magicname         CPerlObj::Perl_magicname
#undef  malloced_size
#define malloced_size     CPerlObj::Perl_malloced_size
#undef  markstack_grow
#define markstack_grow    CPerlObj::Perl_markstack_grow
#undef  markstack_ptr
#define markstack_ptr     CPerlObj::Perl_markstack_ptr
#undef  mess
#define mess              CPerlObj::Perl_mess
#undef  mess_alloc
#define mess_alloc        CPerlObj::mess_alloc
#undef  mem_collxfrm
#define mem_collxfrm      CPerlObj::Perl_mem_collxfrm
#undef  mg_clear
#define mg_clear          CPerlObj::Perl_mg_clear
#undef  mg_copy
#define mg_copy           CPerlObj::Perl_mg_copy
#undef  mg_find
#define mg_find           CPerlObj::Perl_mg_find
#undef  mg_free
#define mg_free           CPerlObj::Perl_mg_free
#undef  mg_get
#define mg_get            CPerlObj::Perl_mg_get
#undef  mg_length
#define mg_length         CPerlObj::Perl_mg_length
#undef  mg_magical
#define mg_magical        CPerlObj::Perl_mg_magical
#undef  mg_set
#define mg_set            CPerlObj::Perl_mg_set
#undef  mg_size
#define mg_size           CPerlObj::Perl_mg_size
#undef  missingterm
#define missingterm       CPerlObj::missingterm
#undef  mod
#define mod               CPerlObj::Perl_mod
#undef  modkids
#define modkids           CPerlObj::Perl_modkids
#undef  moreswitches
#define moreswitches      CPerlObj::Perl_moreswitches
#undef  more_sv
#define more_sv           CPerlObj::more_sv
#undef  more_xiv
#define more_xiv          CPerlObj::more_xiv
#undef  more_xnv
#define more_xnv          CPerlObj::more_xnv
#undef  more_xpv
#define more_xpv          CPerlObj::more_xpv
#undef  more_xrv
#define more_xrv          CPerlObj::more_xrv
#undef  mstats
#define mstats            CPerlObj::mstats
#undef  mul128
#define mul128            CPerlObj::mul128
#undef  my
#define my                CPerlObj::Perl_my
#undef  my_bcopy
#define my_bcopy          CPerlObj::Perl_my_bcopy
#undef  my_bzero
#define my_bzero          CPerlObj::Perl_my_bzero
#undef  my_exit
#define my_exit           CPerlObj::Perl_my_exit
#undef  my_exit_jump
#define my_exit_jump      CPerlObj::my_exit_jump
#undef  my_failure_exit
#define my_failure_exit   CPerlObj::Perl_my_failure_exit
#undef  my_lstat
#define my_lstat          CPerlObj::Perl_my_lstat
#undef  my_memcmp
#define my_memcmp         CPerlObj::Perl_my_memcmp
#undef  my_memset
#define my_memset         CPerlObj::Perl_my_memset
#undef  my_pclose
#define my_pclose         CPerlObj::Perl_my_pclose
#undef  my_popen
#define my_popen          CPerlObj::Perl_my_popen
#undef  my_safemalloc
#define my_safemalloc     CPerlObj::my_safemalloc
#undef  my_setenv
#define my_setenv         CPerlObj::Perl_my_setenv
#undef  my_stat
#define my_stat           CPerlObj::Perl_my_stat
#undef  my_swap
#define my_swap           CPerlObj::my_swap
#undef  my_htonl
#define my_htonl          CPerlObj::my_htonl
#undef  my_ntohl
#define my_ntohl          CPerlObj::my_ntohl
#undef  my_unexec
#define my_unexec         CPerlObj::Perl_my_unexec
#undef  newANONLIST
#define newANONLIST       CPerlObj::Perl_newANONLIST
#undef  newANONHASH
#define newANONHASH       CPerlObj::Perl_newANONHASH
#undef  newANONSUB
#define newANONSUB        CPerlObj::Perl_newANONSUB
#undef  newASSIGNOP
#define newASSIGNOP       CPerlObj::Perl_newASSIGNOP
#undef  newCONDOP
#define newCONDOP         CPerlObj::Perl_newCONDOP
#undef  newCONSTSUB
#define newCONSTSUB       CPerlObj::Perl_newCONSTSUB
#undef  newDEFSVOP
#define newDEFSVOP        CPerlObj::newDEFSVOP
#undef  newFORM
#define newFORM           CPerlObj::Perl_newFORM
#undef  newFOROP
#define newFOROP          CPerlObj::Perl_newFOROP
#undef  newLOGOP
#define newLOGOP          CPerlObj::Perl_newLOGOP
#undef  newLOOPEX
#define newLOOPEX         CPerlObj::Perl_newLOOPEX
#undef  newLOOPOP
#define newLOOPOP         CPerlObj::Perl_newLOOPOP
#undef  newMETHOD
#define newMETHOD         CPerlObj::Perl_newMETHOD
#undef  newNULLLIST
#define newNULLLIST       CPerlObj::Perl_newNULLLIST
#undef  newOP
#define newOP             CPerlObj::Perl_newOP
#undef  newPROG
#define newPROG           CPerlObj::Perl_newPROG
#undef  newRANGE
#define newRANGE          CPerlObj::Perl_newRANGE
#undef  newSLICEOP
#define newSLICEOP        CPerlObj::Perl_newSLICEOP
#undef  newSTATEOP
#define newSTATEOP        CPerlObj::Perl_newSTATEOP
#undef  newSUB
#define newSUB            CPerlObj::Perl_newSUB
#undef  newXS
#define newXS             CPerlObj::Perl_newXS
#undef  newXSUB
#define newXSUB           CPerlObj::Perl_newXSUB
#undef  newAV
#define newAV             CPerlObj::Perl_newAV
#undef  newAVREF
#define newAVREF          CPerlObj::Perl_newAVREF
#undef  newBINOP
#define newBINOP          CPerlObj::Perl_newBINOP
#undef  newCVREF
#define newCVREF          CPerlObj::Perl_newCVREF
#undef  newCVOP
#define newCVOP           CPerlObj::Perl_newCVOP
#undef  newGVOP
#define newGVOP           CPerlObj::Perl_newGVOP
#undef  newGVgen
#define newGVgen          CPerlObj::Perl_newGVgen
#undef  newGVREF
#define newGVREF          CPerlObj::Perl_newGVREF
#undef  newHVREF
#define newHVREF          CPerlObj::Perl_newHVREF
#undef  newHV
#define newHV             CPerlObj::Perl_newHV
#undef  newHVhv
#define newHVhv           CPerlObj::Perl_newHVhv
#undef  newIO
#define newIO             CPerlObj::Perl_newIO
#undef  newLISTOP
#define newLISTOP         CPerlObj::Perl_newLISTOP
#undef  newPMOP
#define newPMOP           CPerlObj::Perl_newPMOP
#undef  newPVOP
#define newPVOP           CPerlObj::Perl_newPVOP
#undef  newRV
#define newRV             CPerlObj::Perl_newRV
#undef  Perl_newRV_noinc
#define Perl_newRV_noinc  CPerlObj::Perl_newRV_noinc
#undef  newSV
#define newSV             CPerlObj::Perl_newSV
#undef  newSVREF
#define newSVREF          CPerlObj::Perl_newSVREF
#undef  newSVOP
#define newSVOP           CPerlObj::Perl_newSVOP
#undef  newSViv
#define newSViv           CPerlObj::Perl_newSViv
#undef  newSVnv
#define newSVnv           CPerlObj::Perl_newSVnv
#undef  newSVpv
#define newSVpv           CPerlObj::Perl_newSVpv
#undef  newSVpvf
#define newSVpvf          CPerlObj::Perl_newSVpvf
#undef  newSVpvn
#define newSVpvn          CPerlObj::Perl_newSVpvn
#undef  newSVrv
#define newSVrv           CPerlObj::Perl_newSVrv
#undef  newSVsv
#define newSVsv           CPerlObj::Perl_newSVsv
#undef  newUNOP
#define newUNOP           CPerlObj::Perl_newUNOP
#undef  newWHILEOP
#define newWHILEOP        CPerlObj::Perl_newWHILEOP
#undef  new_constant
#define new_constant      CPerlObj::new_constant
#undef  new_logop
#define new_logop         CPerlObj::new_logop
#undef  new_stackinfo
#define new_stackinfo     CPerlObj::Perl_new_stackinfo
#undef  new_sv
#define new_sv            CPerlObj::new_sv
#undef  new_xiv
#define new_xiv           CPerlObj::new_xiv
#undef  new_xnv
#define new_xnv           CPerlObj::new_xnv
#undef  new_xpv
#define new_xpv           CPerlObj::new_xpv
#undef  new_xrv
#define new_xrv           CPerlObj::new_xrv
#undef  nextargv
#define nextargv          CPerlObj::Perl_nextargv
#undef  nextchar
#define nextchar          CPerlObj::nextchar
#undef  ninstr
#define ninstr            CPerlObj::Perl_ninstr
#undef  not_a_number
#define not_a_number      CPerlObj::not_a_number
#undef  no_fh_allowed
#define no_fh_allowed     CPerlObj::Perl_no_fh_allowed
#undef  no_op
#define no_op             CPerlObj::Perl_no_op
#undef  null
#define null              CPerlObj::null
#undef  profiledata
#define profiledata       CPerlObj::Perl_profiledata
#undef  package
#define package           CPerlObj::Perl_package
#undef  pad_alloc
#define pad_alloc         CPerlObj::Perl_pad_alloc
#undef  pad_allocmy
#define pad_allocmy       CPerlObj::Perl_pad_allocmy
#undef  pad_findmy
#define pad_findmy        CPerlObj::Perl_pad_findmy
#undef  op_const_sv
#define op_const_sv       CPerlObj::Perl_op_const_sv
#undef  op_free
#define op_free           CPerlObj::Perl_op_free
#undef  oopsCV
#define oopsCV            CPerlObj::Perl_oopsCV
#undef  oopsAV
#define oopsAV            CPerlObj::Perl_oopsAV
#undef  oopsHV
#define oopsHV            CPerlObj::Perl_oopsHV
#undef  open_script
#define open_script       CPerlObj::open_script
#undef  pad_leavemy
#define pad_leavemy       CPerlObj::Perl_pad_leavemy
#undef  pad_sv
#define pad_sv            CPerlObj::Perl_pad_sv
#undef  pad_findlex
#define pad_findlex       CPerlObj::pad_findlex
#undef  pad_free
#define pad_free          CPerlObj::Perl_pad_free
#undef  pad_reset
#define pad_reset         CPerlObj::Perl_pad_reset
#undef  pad_swipe
#define pad_swipe         CPerlObj::Perl_pad_swipe
#undef  peep
#define peep              CPerlObj::Perl_peep
#undef  perl_call_argv
#define perl_call_argv    CPerlObj::perl_call_argv
#undef  perl_call_method
#define perl_call_method  CPerlObj::perl_call_method
#undef  perl_call_pv
#define perl_call_pv      CPerlObj::perl_call_pv
#undef  perl_call_sv
#define perl_call_sv      CPerlObj::perl_call_sv
#undef  perl_callargv
#define perl_callargv     CPerlObj::perl_callargv
#undef  perl_callpv
#define perl_callpv       CPerlObj::perl_callpv
#undef  perl_callsv
#define perl_callsv       CPerlObj::perl_callsv
#undef  perl_eval_pv
#define perl_eval_pv      CPerlObj::perl_eval_pv
#undef  perl_eval_sv
#define perl_eval_sv      CPerlObj::perl_eval_sv
#undef  perl_get_sv
#define perl_get_sv       CPerlObj::perl_get_sv
#undef  perl_get_av
#define perl_get_av       CPerlObj::perl_get_av
#undef  perl_get_hv
#define perl_get_hv       CPerlObj::perl_get_hv
#undef  perl_get_cv
#define perl_get_cv       CPerlObj::perl_get_cv
#undef  Perl_GetVars
#define Perl_GetVars      CPerlObj::Perl_GetVars
#undef  perl_init_fold
#define perl_init_fold    CPerlObj::perl_init_fold
#undef  perl_init_i18nl10n
#define perl_init_i18nl10n CPerlObj::perl_init_i18nl10n
#undef  perl_init_i18nl14n
#define perl_init_i18nl14n CPerlObj::perl_init_i18nl14n
#undef  perl_new_collate
#define perl_new_collate  CPerlObj::perl_new_collate
#undef  perl_new_ctype
#define perl_new_ctype    CPerlObj::perl_new_ctype
#undef  perl_new_numeric
#define perl_new_numeric  CPerlObj::perl_new_numeric
#undef  perl_set_numeric_standard
#define perl_set_numeric_standard CPerlObj::perl_set_numeric_standard
#undef  perl_set_numeric_local
#define perl_set_numeric_local CPerlObj::perl_set_numeric_local
#undef  perl_require_pv
#define perl_require_pv   CPerlObj::perl_require_pv
#undef  perl_thread
#define perl_thread       CPerlObj::perl_thread
#undef  pidgone
#define pidgone           CPerlObj::Perl_pidgone
#undef  pmflag
#define pmflag            CPerlObj::Perl_pmflag
#undef  pmruntime
#define pmruntime         CPerlObj::Perl_pmruntime
#undef  pmtrans
#define pmtrans           CPerlObj::Perl_pmtrans
#undef  pop_return
#define pop_return        CPerlObj::Perl_pop_return
#undef  pop_scope
#define pop_scope         CPerlObj::Perl_pop_scope
#undef  prepend_elem
#define prepend_elem      CPerlObj::Perl_prepend_elem
#undef  provide_ref
#define provide_ref       CPerlObj::Perl_provide_ref
#undef  push_return
#define push_return       CPerlObj::Perl_push_return
#undef  push_scope
#define push_scope        CPerlObj::Perl_push_scope
#undef  pregcomp
#define pregcomp          CPerlObj::Perl_pregcomp
#undef  qsortsv
#define qsortsv           CPerlObj::qsortsv
#undef  ref
#define ref               CPerlObj::Perl_ref
#undef  refkids
#define refkids           CPerlObj::Perl_refkids
#undef  regdump
#define regdump           CPerlObj::Perl_regdump
#undef  rsignal
#define rsignal           CPerlObj::Perl_rsignal
#undef  rsignal_restore
#define rsignal_restore   CPerlObj::Perl_rsignal_restore
#undef  rsignal_save
#define rsignal_save      CPerlObj::Perl_rsignal_save
#undef  rsignal_state
#define rsignal_state     CPerlObj::Perl_rsignal_state
#undef  pregexec
#define pregexec          CPerlObj::Perl_pregexec
#undef  pregfree
#define pregfree          CPerlObj::Perl_pregfree
#undef  re_croak2
#define re_croak2         CPerlObj::re_croak2
#undef  refto
#define refto             CPerlObj::refto
#undef  reg
#define reg               CPerlObj::reg
#undef  reg_node
#define reg_node          CPerlObj::reg_node
#undef  reganode
#define reganode          CPerlObj::reganode
#undef  regatom
#define regatom           CPerlObj::regatom
#undef  regbranch
#define regbranch         CPerlObj::regbranch
#undef  regc
#define regc              CPerlObj::regc
#undef  regcurly
#define regcurly          CPerlObj::regcurly
#undef  regcppush
#define regcppush         CPerlObj::regcppush
#undef  regcppop
#define regcppop          CPerlObj::regcppop
#undef  regclass
#define regclass          CPerlObj::regclass
#undef  regexec_flags
#define regexec_flags     CPerlObj::Perl_regexec_flags
#undef  reginclass
#define reginclass        CPerlObj::reginclass
#undef  reginsert
#define reginsert         CPerlObj::reginsert
#undef  regmatch
#define regmatch          CPerlObj::regmatch
#undef  regnext
#define regnext           CPerlObj::Perl_regnext
#undef  regoptail
#define regoptail         CPerlObj::regoptail
#undef  regpiece
#define regpiece          CPerlObj::regpiece
#undef  regprop
#define regprop           CPerlObj::Perl_regprop
#undef  regrepeat
#define regrepeat         CPerlObj::regrepeat
#undef  regrepeat_hard
#define regrepeat_hard    CPerlObj::regrepeat_hard
#undef  regset
#define regset            CPerlObj::regset
#undef  regtail
#define regtail           CPerlObj::regtail
#undef  regtry
#define regtry            CPerlObj::regtry
#undef  regwhite
#define regwhite          CPerlObj::regwhite
#undef  repeatcpy
#define repeatcpy         CPerlObj::Perl_repeatcpy
#undef  restore_expect
#define restore_expect    CPerlObj::restore_expect
#undef  restore_lex_expect
#define restore_lex_expect CPerlObj::restore_lex_expect
#undef  restore_magic
#define restore_magic     CPerlObj::restore_magic
#undef  restore_rsfp
#define restore_rsfp      CPerlObj::restore_rsfp
#undef  rninstr
#define rninstr           CPerlObj::Perl_rninstr
#undef  runops_standard
#define runops_standard   CPerlObj::Perl_runops_standard
#undef  runops_debug
#define runops_debug      CPerlObj::Perl_runops_debug
#undef  rxres_free
#define rxres_free        CPerlObj::Perl_rxres_free
#undef  rxres_restore
#define rxres_restore     CPerlObj::Perl_rxres_restore
#undef  rxres_save
#define rxres_save        CPerlObj::Perl_rxres_save
#ifndef MYMALLOC
#undef  safefree
#define safefree          CPerlObj::Perl_safefree
#undef  safecalloc
#define safecalloc        CPerlObj::Perl_safecalloc
#undef  safemalloc
#define safemalloc        CPerlObj::Perl_safemalloc
#undef  saferealloc
#define saferealloc       CPerlObj::Perl_saferealloc
#endif	/* MYMALLOC */
#undef  same_dirent
#define same_dirent       CPerlObj::same_dirent
#undef  savepv
#define savepv            CPerlObj::Perl_savepv
#undef  savepvn
#define savepvn           CPerlObj::Perl_savepvn
#undef  savestack_grow
#define savestack_grow    CPerlObj::Perl_savestack_grow
#undef  save_aelem
#define save_aelem        CPerlObj::Perl_save_aelem
#undef  save_aptr
#define save_aptr         CPerlObj::Perl_save_aptr
#undef  save_ary
#define save_ary          CPerlObj::Perl_save_ary
#undef  save_clearsv
#define save_clearsv      CPerlObj::Perl_save_clearsv
#undef  save_delete
#define save_delete       CPerlObj::Perl_save_delete
#undef  save_destructor
#define save_destructor   CPerlObj::Perl_save_destructor
#undef  save_freesv
#define save_freesv       CPerlObj::Perl_save_freesv
#undef  save_freeop
#define save_freeop       CPerlObj::Perl_save_freeop
#undef  save_freepv
#define save_freepv       CPerlObj::Perl_save_freepv
#undef  save_generic_svref
#define save_generic_svref CPerlObj::Perl_save_generic_svref
#undef  save_gp
#define save_gp           CPerlObj::Perl_save_gp
#undef  save_hash
#define save_hash         CPerlObj::Perl_save_hash
#undef  save_hek
#define save_hek          CPerlObj::save_hek
#undef  save_helem
#define save_helem        CPerlObj::Perl_save_helem
#undef  save_hints
#define save_hints        CPerlObj::Perl_save_hints
#undef  save_hptr
#define save_hptr         CPerlObj::Perl_save_hptr
#undef  save_I16
#define save_I16          CPerlObj::Perl_save_I16
#undef  save_I32
#define save_I32          CPerlObj::Perl_save_I32
#undef  save_int
#define save_int          CPerlObj::Perl_save_int
#undef  save_item
#define save_item         CPerlObj::Perl_save_item
#undef  save_iv
#define save_iv           CPerlObj::Perl_save_iv
#undef  save_lines
#define save_lines        CPerlObj::save_lines
#undef  save_list
#define save_list         CPerlObj::Perl_save_list
#undef  save_long
#define save_long         CPerlObj::Perl_save_long
#undef  save_magic
#define save_magic        CPerlObj::save_magic
#undef  save_nogv
#define save_nogv         CPerlObj::Perl_save_nogv
#undef  save_op
#define save_op           CPerlObj::Perl_save_op
#undef  save_scalar
#define save_scalar       CPerlObj::Perl_save_scalar
#undef  save_scalar_at
#define save_scalar_at    CPerlObj::save_scalar_at
#undef  save_pptr
#define save_pptr         CPerlObj::Perl_save_pptr
#undef  save_sptr
#define save_sptr         CPerlObj::Perl_save_sptr
#undef  save_svref
#define save_svref        CPerlObj::Perl_save_svref
#undef  save_threadsv
#define save_threadsv     CPerlObj::Perl_save_threadsv
#undef  sawparens
#define sawparens         CPerlObj::Perl_sawparens
#undef  scalar
#define scalar            CPerlObj::Perl_scalar
#undef  scalarboolean
#define scalarboolean     CPerlObj::scalarboolean
#undef  scalarkids
#define scalarkids        CPerlObj::Perl_scalarkids
#undef  scalarseq
#define scalarseq         CPerlObj::Perl_scalarseq
#undef  scalarvoid
#define scalarvoid        CPerlObj::Perl_scalarvoid
#undef  scan_commit
#define scan_commit       CPerlObj::scan_commit
#undef  scan_const
#define scan_const        CPerlObj::Perl_scan_const
#undef  scan_formline
#define scan_formline     CPerlObj::Perl_scan_formline
#undef  scan_ident
#define scan_ident        CPerlObj::Perl_scan_ident
#undef  scan_inputsymbol
#define scan_inputsymbol  CPerlObj::Perl_scan_inputsymbol
#undef  scan_heredoc
#define scan_heredoc      CPerlObj::Perl_scan_heredoc
#undef  scan_hex
#define scan_hex          CPerlObj::Perl_scan_hex
#undef  scan_num
#define scan_num          CPerlObj::Perl_scan_num
#undef  scan_oct
#define scan_oct          CPerlObj::Perl_scan_oct
#undef  scan_pat
#define scan_pat          CPerlObj::Perl_scan_pat
#undef  scan_str
#define scan_str          CPerlObj::Perl_scan_str
#undef  scan_subst
#define scan_subst        CPerlObj::Perl_scan_subst
#undef  scan_trans
#define scan_trans        CPerlObj::Perl_scan_trans
#undef  scan_word
#define scan_word         CPerlObj::Perl_scan_word
#undef  scope
#define scope             CPerlObj::Perl_scope
#undef  screaminstr
#define screaminstr       CPerlObj::Perl_screaminstr
#undef  seed
#define seed              CPerlObj::seed
#undef  setdefout
#define setdefout         CPerlObj::Perl_setdefout
#undef  setenv_getix
#define setenv_getix      CPerlObj::Perl_setenv_getix
#undef  sharepvn
#define sharepvn          CPerlObj::Perl_sharepvn
#undef  set_csh
#define set_csh           CPerlObj::set_csh
#undef  sighandler
#define sighandler        CPerlObj::Perl_sighandler
#undef  share_hek
#define share_hek         CPerlObj::Perl_share_hek
#undef  skipspace
#define skipspace         CPerlObj::Perl_skipspace
#undef  sortcv
#define sortcv            CPerlObj::sortcv
#ifndef PERL_OBJECT
#undef  stack_base
#define stack_base        CPerlObj::Perl_stack_base
#endif
#undef  stack_grow
#define stack_grow        CPerlObj::Perl_stack_grow
#undef  start_subparse
#define start_subparse    CPerlObj::Perl_start_subparse
#undef  study_chunk
#define study_chunk       CPerlObj::study_chunk
#undef  sub_crush_depth
#define sub_crush_depth   CPerlObj::Perl_sub_crush_depth
#undef  sublex_done
#define sublex_done       CPerlObj::sublex_done
#undef  sublex_push
#define sublex_push       CPerlObj::sublex_push
#undef  sublex_start
#define sublex_start      CPerlObj::sublex_start
#undef  sv_2bool
#define sv_2bool          CPerlObj::Perl_sv_2bool
#undef  sv_2cv
#define sv_2cv            CPerlObj::Perl_sv_2cv
#undef  sv_2io
#define sv_2io            CPerlObj::Perl_sv_2io
#undef  sv_2iv
#define sv_2iv            CPerlObj::Perl_sv_2iv
#undef  sv_2uv
#define sv_2uv            CPerlObj::Perl_sv_2uv
#undef  sv_2mortal
#define sv_2mortal        CPerlObj::Perl_sv_2mortal
#undef  sv_2nv
#define sv_2nv            CPerlObj::Perl_sv_2nv
#undef  sv_2pv
#define sv_2pv            CPerlObj::Perl_sv_2pv
#undef  sv_add_arena
#define sv_add_arena      CPerlObj::Perl_sv_add_arena
#undef  sv_backoff
#define sv_backoff        CPerlObj::Perl_sv_backoff
#undef  sv_bless
#define sv_bless          CPerlObj::Perl_sv_bless
#undef  sv_catpv
#define sv_catpv          CPerlObj::Perl_sv_catpv
#undef  sv_catpv_mg
#define sv_catpv_mg       CPerlObj::Perl_sv_catpv_mg
#undef  sv_catpvf
#define sv_catpvf         CPerlObj::Perl_sv_catpvf
#undef  sv_catpvf_mg
#define sv_catpvf_mg      CPerlObj::Perl_sv_catpvf_mg
#undef  sv_catpvn
#define sv_catpvn         CPerlObj::Perl_sv_catpvn
#undef  sv_catpvn_mg
#define sv_catpvn_mg      CPerlObj::Perl_sv_catpvn_mg
#undef  sv_catsv
#define sv_catsv          CPerlObj::Perl_sv_catsv
#undef  sv_catsv_mg
#define sv_catsv_mg       CPerlObj::Perl_sv_catsv_mg
#undef  sv_check_thinkfirst
#define sv_check_thinkfirst CPerlObj::sv_check_thinkfirst
#undef  sv_chop
#define sv_chop           CPerlObj::Perl_sv_chop
#undef  sv_clean_all
#define sv_clean_all      CPerlObj::Perl_sv_clean_all
#undef  sv_clean_objs
#define sv_clean_objs     CPerlObj::Perl_sv_clean_objs
#undef  sv_clear
#define sv_clear          CPerlObj::Perl_sv_clear
#undef  sv_cmp
#define sv_cmp            CPerlObj::Perl_sv_cmp
#undef  sv_cmp_locale
#define sv_cmp_locale     CPerlObj::Perl_sv_cmp_locale
#undef  sv_collxfrm
#define sv_collxfrm       CPerlObj::Perl_sv_collxfrm
#undef  sv_compile_2op
#define sv_compile_2op    CPerlObj::Perl_sv_compile_2op
#undef  sv_dec
#define sv_dec            CPerlObj::Perl_sv_dec
#undef  sv_derived_from
#define sv_derived_from   CPerlObj::Perl_sv_derived_from
#undef  sv_dump
#define sv_dump           CPerlObj::Perl_sv_dump
#undef  sv_eq
#define sv_eq             CPerlObj::Perl_sv_eq
#undef  sv_free
#define sv_free           CPerlObj::Perl_sv_free
#undef  sv_free_arenas
#define sv_free_arenas    CPerlObj::Perl_sv_free_arenas
#undef  sv_gets
#define sv_gets           CPerlObj::Perl_sv_gets
#undef  sv_grow
#define sv_grow           CPerlObj::Perl_sv_grow
#undef  sv_inc
#define sv_inc            CPerlObj::Perl_sv_inc
#undef  sv_insert
#define sv_insert         CPerlObj::Perl_sv_insert
#undef  sv_isa
#define sv_isa            CPerlObj::Perl_sv_isa
#undef  sv_isobject
#define sv_isobject       CPerlObj::Perl_sv_isobject
#undef  sv_iv
#define sv_iv             CPerlObj::Perl_sv_iv
#undef  sv_len
#define sv_len            CPerlObj::Perl_sv_len
#undef  sv_magic
#define sv_magic          CPerlObj::Perl_sv_magic
#undef  sv_mortalcopy
#define sv_mortalcopy     CPerlObj::Perl_sv_mortalcopy
#undef  sv_mortalgrow
#define sv_mortalgrow     CPerlObj::sv_mortalgrow
#undef  sv_newmortal
#define sv_newmortal      CPerlObj::Perl_sv_newmortal
#undef  sv_newref
#define sv_newref         CPerlObj::Perl_sv_newref
#undef  sv_nv
#define sv_nv             CPerlObj::Perl_sv_nv
#undef  sv_peek
#define sv_peek           CPerlObj::Perl_sv_peek
#undef  sv_pvn
#define sv_pvn            CPerlObj::Perl_sv_pvn
#undef  sv_pvn_force
#define sv_pvn_force      CPerlObj::Perl_sv_pvn_force
#undef  sv_reftype
#define sv_reftype        CPerlObj::Perl_sv_reftype
#undef  sv_replace
#define sv_replace        CPerlObj::Perl_sv_replace
#undef  sv_report_used
#define sv_report_used    CPerlObj::Perl_sv_report_used
#undef  sv_reset
#define sv_reset          CPerlObj::Perl_sv_reset
#undef  sv_setiv
#define sv_setiv          CPerlObj::Perl_sv_setiv
#undef  sv_setiv_mg
#define sv_setiv_mg       CPerlObj::Perl_sv_setiv_mg
#undef  sv_setnv
#define sv_setnv          CPerlObj::Perl_sv_setnv
#undef  sv_setnv_mg
#define sv_setnv_mg       CPerlObj::Perl_sv_setnv_mg
#undef  sv_setuv
#define sv_setuv          CPerlObj::Perl_sv_setuv
#undef  sv_setuv_mg
#define sv_setuv_mg       CPerlObj::Perl_sv_setuv_mg
#undef  sv_setref_iv
#define sv_setref_iv      CPerlObj::Perl_sv_setref_iv
#undef  sv_setref_nv
#define sv_setref_nv      CPerlObj::Perl_sv_setref_nv
#undef  sv_setref_pv
#define sv_setref_pv      CPerlObj::Perl_sv_setref_pv
#undef  sv_setref_pvn
#define sv_setref_pvn     CPerlObj::Perl_sv_setref_pvn
#undef  sv_setpv
#define sv_setpv          CPerlObj::Perl_sv_setpv
#undef  sv_setpv_mg
#define sv_setpv_mg       CPerlObj::Perl_sv_setpv_mg
#undef  sv_setpvf
#define sv_setpvf         CPerlObj::Perl_sv_setpvf
#undef  sv_setpvf_mg
#define sv_setpvf_mg      CPerlObj::Perl_sv_setpvf_mg
#undef  sv_setpviv
#define sv_setpviv        CPerlObj::Perl_sv_setpviv
#undef  sv_setpviv_mg
#define sv_setpviv_mg     CPerlObj::Perl_sv_setpviv_mg
#undef  sv_setpvn
#define sv_setpvn         CPerlObj::Perl_sv_setpvn
#undef  sv_setpvn_mg
#define sv_setpvn_mg      CPerlObj::Perl_sv_setpvn_mg
#undef  sv_setsv
#define sv_setsv          CPerlObj::Perl_sv_setsv
#undef  sv_setsv_mg
#define sv_setsv_mg       CPerlObj::Perl_sv_setsv_mg
#undef  sv_taint
#define sv_taint          CPerlObj::Perl_sv_taint
#undef  sv_tainted
#define sv_tainted        CPerlObj::Perl_sv_tainted
#undef  sv_true
#define sv_true           CPerlObj::Perl_sv_true
#undef  sv_unglob
#define sv_unglob         CPerlObj::sv_unglob
#undef  sv_unmagic
#define sv_unmagic        CPerlObj::Perl_sv_unmagic
#undef  sv_unref
#define sv_unref          CPerlObj::Perl_sv_unref
#undef  sv_untaint
#define sv_untaint        CPerlObj::Perl_sv_untaint
#undef  sv_upgrade
#define sv_upgrade        CPerlObj::Perl_sv_upgrade
#undef  sv_usepvn
#define sv_usepvn         CPerlObj::Perl_sv_usepvn
#undef  sv_usepvn_mg
#define sv_usepvn_mg      CPerlObj::Perl_sv_usepvn_mg
#undef  sv_uv
#define sv_uv             CPerlObj::Perl_sv_uv
#undef  sv_vcatpvfn
#define sv_vcatpvfn       CPerlObj::Perl_sv_vcatpvfn
#undef  sv_vsetpvfn
#define sv_vsetpvfn       CPerlObj::Perl_sv_vsetpvfn
#undef  taint_env
#define taint_env         CPerlObj::Perl_taint_env
#undef  taint_not
#define taint_not         CPerlObj::Perl_taint_not
#undef  taint_proper
#define taint_proper      CPerlObj::Perl_taint_proper
#undef  tokeq
#define tokeq             CPerlObj::tokeq
#undef  too_few_arguments
#define too_few_arguments CPerlObj::Perl_too_few_arguments
#undef  too_many_arguments
#define too_many_arguments CPerlObj::Perl_too_many_arguments
#undef  unlnk
#define unlnk             CPerlObj::unlnk
#undef  unsharepvn
#define unsharepvn        CPerlObj::Perl_unsharepvn
#undef  unshare_hek
#define unshare_hek       CPerlObj::Perl_unshare_hek
#undef  unwind_handler_stack
#define unwind_handler_stack CPerlObj::unwind_handler_stack
#undef  usage
#define usage             CPerlObj::usage
#undef  utilize
#define utilize           CPerlObj::Perl_utilize
#undef  validate_suid
#define validate_suid     CPerlObj::validate_suid
#undef  visit
#define visit             CPerlObj::visit
#undef  vivify_defelem
#define vivify_defelem    CPerlObj::Perl_vivify_defelem
#undef  vivify_ref
#define vivify_ref        CPerlObj::Perl_vivify_ref
#undef  wait4pid
#define wait4pid          CPerlObj::Perl_wait4pid
#undef  warn
#define warn              CPerlObj::Perl_warn
#undef  watch
#define watch             CPerlObj::Perl_watch
#undef  whichsig
#define whichsig          CPerlObj::Perl_whichsig
#undef  win32_textfilter
#define win32_textfilter  CPerlObj::win32_textfilter
#undef  yyerror
#define yyerror           CPerlObj::Perl_yyerror
#undef  yylex
#define yylex             CPerlObj::Perl_yylex
#undef  yyparse
#define yyparse           CPerlObj::Perl_yyparse
#undef  yywarn
#define yywarn            CPerlObj::Perl_yywarn
#undef  yydestruct
#define yydestruct        CPerlObj::Perl_yydestruct

#define new_he            CPerlObj::new_he
#define more_he           CPerlObj::more_he
#define del_he            CPerlObj::del_he

#if defined(WIN32) && !defined(WIN32IO_IS_STDIO)
#undef errno
#define errno             CPerlObj::ErrorNo()

#endif /* WIN32 */

#endif /* __Objpp_h__ */
