#ifndef __objXSUB_h__
#define __objXSUB_h__

/* Varibles */ 

#undef  PL_Sv			
#define PL_Sv					pPerl->PL_Sv			
#undef  PL_Xpv			
#define PL_Xpv					pPerl->PL_Xpv			
#undef  PL_av_fetch_sv		
#define PL_av_fetch_sv			pPerl->PL_av_fetch_sv		
#undef  PL_bodytarget		
#define PL_bodytarget				pPerl->PL_bodytarget		
#undef  PL_bostr		
#define PL_bostr				pPerl->PL_bostr		
#undef  PL_chopset		
#define PL_chopset				pPerl->PL_chopset		
#undef  PL_colors		
#define PL_colors				pPerl->PL_colors		
#undef  PL_colorset		
#define PL_colorset				pPerl->PL_colorset		
#undef  PL_cred_mutex
#define PL_cred_mutex				pPerl->PL_cred_mutex   
#undef  PL_curcop		
#define PL_curcop				pPerl->PL_curcop		
#undef  PL_curpad		
#define PL_curpad				pPerl->PL_curpad		
#undef  PL_curpm		
#define PL_curpm				pPerl->PL_curpm		
#undef  PL_curstack		
#define PL_curstack				pPerl->PL_curstack		
#undef  PL_curstackinfo		
#define PL_curstackinfo			pPerl->PL_curstackinfo		
#undef  PL_curstash		
#define PL_curstash				pPerl->PL_curstash		
#undef  PL_defoutgv		
#define PL_defoutgv				pPerl->PL_defoutgv		
#undef  PL_defstash		
#define PL_defstash				pPerl->PL_defstash		
#undef  PL_delaymagic		
#define PL_delaymagic				pPerl->PL_delaymagic		
#undef  PL_dirty		
#define PL_dirty				pPerl->PL_dirty		
#undef  PL_extralen		
#define PL_extralen				pPerl->PL_extralen		
#undef  PL_firstgv		
#define PL_firstgv				pPerl->PL_firstgv		
#undef  PL_formtarget		
#define PL_formtarget				pPerl->PL_formtarget		
#undef  PL_hv_fetch_ent_mh	
#define PL_hv_fetch_ent_mh		pPerl->PL_hv_fetch_ent_mh	
#undef  PL_hv_fetch_sv		
#define PL_hv_fetch_sv			pPerl->PL_hv_fetch_sv		
#undef  PL_in_eval		
#define PL_in_eval				pPerl->PL_in_eval		
#undef  PL_last_in_gv		
#define PL_last_in_gv				pPerl->PL_last_in_gv		
#undef  PL_lastgotoprobe	
#define PL_lastgotoprobe		pPerl->PL_lastgotoprobe	
#undef  PL_lastscream		
#define PL_lastscream				pPerl->PL_lastscream		
#undef  PL_localizing		
#define PL_localizing				pPerl->PL_localizing		
#undef  PL_mainstack		
#define PL_mainstack				pPerl->PL_mainstack		
#undef  PL_markstack		
#define PL_markstack				pPerl->PL_markstack		
#undef  PL_markstack_max	
#define PL_markstack_max		pPerl->PL_markstack_max	
#undef  PL_markstack_ptr	
#define PL_markstack_ptr		pPerl->PL_markstack_ptr	
#undef  PL_maxscream		
#define PL_maxscream				pPerl->PL_maxscream		
#undef  PL_modcount		
#define PL_modcount				pPerl->PL_modcount		
#undef  PL_nrs			
#define PL_nrs					pPerl->PL_nrs			
#undef  PL_ofs			
#define PL_ofs					pPerl->PL_ofs			
#undef  PL_ofslen		
#define PL_ofslen				pPerl->PL_ofslen		
#undef  PL_op			
#define PL_op					pPerl->PL_op			
#undef  PL_opsave		
#define PL_opsave				pPerl->PL_opsave		
#undef  PL_reg_eval_set		
#define PL_reg_eval_set			pPerl->PL_reg_eval_set		
#undef  PL_reg_flags		
#define PL_reg_flags				pPerl->PL_reg_flags		
#undef  PL_reg_start_tmp	
#define PL_reg_start_tmp		pPerl->PL_reg_start_tmp	
#undef  PL_reg_start_tmpl	
#define PL_reg_start_tmpl		pPerl->PL_reg_start_tmpl	
#undef  PL_regbol		
#define PL_regbol				pPerl->PL_regbol		
#undef  PL_regcc		
#define PL_regcc				pPerl->PL_regcc		
#undef  PL_regcode		
#define PL_regcode				pPerl->PL_regcode		
#undef  PL_regcomp_parse	
#define PL_regcomp_parse		pPerl->PL_regcomp_parse	
#undef  PL_regcomp_rx		
#define PL_regcomp_rx				pPerl->PL_regcomp_rx		
#undef  PL_regcompp		
#define PL_regcompp				pPerl->PL_regcompp		
#undef  PL_regdata		
#define PL_regdata				pPerl->PL_regdata		
#undef  PL_regdummy		
#define PL_regdummy				pPerl->PL_regdummy		
#undef  PL_regendp		
#define PL_regendp				pPerl->PL_regendp		
#undef  PL_regeol		
#define PL_regeol				pPerl->PL_regeol		
#undef  PL_regexecp		
#define PL_regexecp				pPerl->PL_regexecp		
#undef  PL_regflags		
#define PL_regflags				pPerl->PL_regflags		
#undef  PL_regindent		
#define PL_regindent				pPerl->PL_regindent		
#undef  PL_reginput		
#define PL_reginput				pPerl->PL_reginput		
#undef  PL_reginterp_cnt	
#define PL_reginterp_cnt		pPerl->PL_reginterp_cnt	
#undef  PL_reglastparen		
#define PL_reglastparen			pPerl->PL_reglastparen		
#undef  PL_regnarrate		
#define PL_regnarrate				pPerl->PL_regnarrate		
#undef  PL_regnaughty		
#define PL_regnaughty				pPerl->PL_regnaughty		
#undef  PL_regnpar		
#define PL_regnpar				pPerl->PL_regnpar		
#undef  PL_regprecomp		
#define PL_regprecomp				pPerl->PL_regprecomp		
#undef  PL_regprev		
#define PL_regprev				pPerl->PL_regprev		
#undef  PL_regprogram		
#define PL_regprogram				pPerl->PL_regprogram		
#undef  PL_regsawback		
#define PL_regsawback				pPerl->PL_regsawback		
#undef  PL_regseen		
#define PL_regseen				pPerl->PL_regseen		
#undef  PL_regsize		
#define PL_regsize				pPerl->PL_regsize		
#undef  PL_regstartp		
#define PL_regstartp				pPerl->PL_regstartp		
#undef  PL_regtill		
#define PL_regtill				pPerl->PL_regtill		
#undef  PL_regxend		
#define PL_regxend				pPerl->PL_regxend		
#undef  PL_restartop		
#define PL_restartop				pPerl->PL_restartop		
#undef  PL_retstack		
#define PL_retstack				pPerl->PL_retstack		
#undef  PL_retstack_ix		
#define PL_retstack_ix			pPerl->PL_retstack_ix		
#undef  PL_retstack_max		
#define PL_retstack_max			pPerl->PL_retstack_max		
#undef  PL_rs			
#define PL_rs					pPerl->PL_rs			
#undef  PL_savestack		
#define PL_savestack				pPerl->PL_savestack		
#undef  PL_savestack_ix		
#define PL_savestack_ix			pPerl->PL_savestack_ix		
#undef  PL_savestack_max	
#define PL_savestack_max		pPerl->PL_savestack_max	
#undef  PL_scopestack		
#define PL_scopestack				pPerl->PL_scopestack		
#undef  PL_scopestack_ix	
#define PL_scopestack_ix		pPerl->PL_scopestack_ix	
#undef  PL_scopestack_max	
#define PL_scopestack_max		pPerl->PL_scopestack_max	
#undef  PL_screamfirst		
#define PL_screamfirst			pPerl->PL_screamfirst		
#undef  PL_screamnext		
#define PL_screamnext				pPerl->PL_screamnext		
#undef  PL_secondgv		
#define PL_secondgv				pPerl->PL_secondgv		
#undef  PL_seen_evals		
#define PL_seen_evals				pPerl->PL_seen_evals		
#undef  PL_seen_zerolen		
#define PL_seen_zerolen			pPerl->PL_seen_zerolen		
#undef  PL_sortcop		
#define PL_sortcop				pPerl->PL_sortcop		
#undef  PL_sortcxix		
#define PL_sortcxix				pPerl->PL_sortcxix		
#undef  PL_sortstash		
#define PL_sortstash				pPerl->PL_sortstash		
#undef  PL_stack_base		
#define PL_stack_base				pPerl->PL_stack_base		
#undef  PL_stack_max		
#define PL_stack_max				pPerl->PL_stack_max		
#undef  PL_stack_sp		
#define PL_stack_sp				pPerl->PL_stack_sp		
#undef  PL_start_env		
#define PL_start_env				pPerl->PL_start_env		
#undef  PL_statbuf		
#define PL_statbuf				pPerl->PL_statbuf		
#undef  PL_statcache		
#define PL_statcache				pPerl->PL_statcache		
#undef  PL_statgv		
#define PL_statgv				pPerl->PL_statgv		
#undef  PL_statname		
#define PL_statname				pPerl->PL_statname		
#undef  PL_tainted		
#define PL_tainted				pPerl->PL_tainted		
#undef  PL_timesbuf		
#define PL_timesbuf				pPerl->PL_timesbuf		
#undef  PL_tmps_floor		
#define PL_tmps_floor				pPerl->PL_tmps_floor		
#undef  PL_tmps_ix		
#define PL_tmps_ix				pPerl->PL_tmps_ix		
#undef  PL_tmps_max		
#define PL_tmps_max				pPerl->PL_tmps_max		
#undef  PL_tmps_stack		
#define PL_tmps_stack				pPerl->PL_tmps_stack		
#undef  PL_top_env		
#define PL_top_env				pPerl->PL_top_env		
#undef  PL_toptarget		
#define PL_toptarget				pPerl->PL_toptarget		
#undef  PL_Argv			
#define PL_Argv					pPerl->PL_Argv			
#undef  PL_Cmd			
#define PL_Cmd					pPerl->PL_Cmd			
#undef  PL_DBcv			
#define PL_DBcv					pPerl->PL_DBcv			
#undef  PL_DBgv			
#define PL_DBgv					pPerl->PL_DBgv			
#undef  PL_DBline		
#define PL_DBline				pPerl->PL_DBline		
#undef  PL_DBsignal		
#define PL_DBsignal				pPerl->PL_DBsignal		
#undef  PL_DBsingle		
#define PL_DBsingle				pPerl->PL_DBsingle		
#undef  PL_DBsub		
#define PL_DBsub				pPerl->PL_DBsub		
#undef  PL_DBtrace		
#define PL_DBtrace				pPerl->PL_DBtrace		
#undef  PL_ampergv		
#define PL_ampergv				pPerl->PL_ampergv		
#undef  PL_archpat_auto		
#define PL_archpat_auto			pPerl->PL_archpat_auto		
#undef  PL_argvgv		
#define PL_argvgv				pPerl->PL_argvgv		
#undef  PL_argvoutgv		
#define PL_argvoutgv				pPerl->PL_argvoutgv		
#undef  PL_basetime		
#define PL_basetime				pPerl->PL_basetime		
#undef  PL_beginav		
#define PL_beginav				pPerl->PL_beginav		
#undef  PL_cddir		
#define PL_cddir				pPerl->PL_cddir		
#undef  PL_compcv		
#define PL_compcv				pPerl->PL_compcv		
#undef  PL_compiling		
#define PL_compiling				pPerl->PL_compiling		
#undef  PL_comppad		
#define PL_comppad				pPerl->PL_comppad		
#undef  PL_comppad_name		
#define PL_comppad_name			pPerl->PL_comppad_name		
#undef  PL_comppad_name_fill	
#define PL_comppad_name_fill		pPerl->PL_comppad_name_fill	
#undef  PL_comppad_name_floor	
#define PL_comppad_name_floor		pPerl->PL_comppad_name_floor	
#undef  PL_copline		
#define PL_copline				pPerl->PL_copline		
#undef  PL_curcopdb		
#define PL_curcopdb				pPerl->PL_curcopdb		
#undef  PL_curstname		
#define PL_curstname				pPerl->PL_curstname		
#undef  PL_dbargs		
#define PL_dbargs				pPerl->PL_dbargs		
#undef  PL_debdelim		
#define PL_debdelim				pPerl->PL_debdelim		
#undef  PL_debname		
#define PL_debname				pPerl->PL_debname		
#undef  PL_debstash		
#define PL_debstash				pPerl->PL_debstash		
#undef  PL_defgv		
#define PL_defgv				pPerl->PL_defgv		
#undef  PL_diehook		
#define PL_diehook				pPerl->PL_diehook		
#undef  PL_dlevel		
#define PL_dlevel				pPerl->PL_dlevel		
#undef  PL_dlmax		
#define PL_dlmax				pPerl->PL_dlmax		
#undef  PL_doextract		
#define PL_doextract				pPerl->PL_doextract		
#undef  PL_doswitches		
#define PL_doswitches				pPerl->PL_doswitches		
#undef  PL_dowarn		
#define PL_dowarn				pPerl->PL_dowarn		
#undef  PL_dumplvl		
#define PL_dumplvl				pPerl->PL_dumplvl		
#undef  PL_e_script		
#define PL_e_script				pPerl->PL_e_script		
#undef  PL_endav		
#define PL_endav				pPerl->PL_endav		
#undef  PL_envgv		
#define PL_envgv				pPerl->PL_envgv		
#undef  PL_errgv		
#define PL_errgv				pPerl->PL_errgv		
#undef  PL_eval_root		
#define PL_eval_root				pPerl->PL_eval_root		
#undef  PL_eval_start		
#define PL_eval_start				pPerl->PL_eval_start		
#undef  PL_exitlist		
#define PL_exitlist				pPerl->PL_exitlist		
#undef  PL_exitlistlen		
#define PL_exitlistlen			pPerl->PL_exitlistlen		
#undef  PL_fdpid		
#define PL_fdpid				pPerl->PL_fdpid		
#undef  PL_filemode		
#define PL_filemode				pPerl->PL_filemode		
#undef  PL_forkprocess		
#define PL_forkprocess			pPerl->PL_forkprocess		
#undef  PL_formfeed		
#define PL_formfeed				pPerl->PL_formfeed		
#undef  PL_generation		
#define PL_generation				pPerl->PL_generation		
#undef  PL_gensym		
#define PL_gensym				pPerl->PL_gensym		
#undef  PL_globalstash		
#define PL_globalstash			pPerl->PL_globalstash		
#undef  PL_hintgv		
#define PL_hintgv				pPerl->PL_hintgv		
#undef  PL_in_clean_all		
#define PL_in_clean_all			pPerl->PL_in_clean_all		
#undef  PL_in_clean_objs	
#define PL_in_clean_objs		pPerl->PL_in_clean_objs	
#undef  PL_incgv		
#define PL_incgv				pPerl->PL_incgv		
#undef  PL_initav		
#define PL_initav				pPerl->PL_initav		
#undef  PL_inplace		
#define PL_inplace				pPerl->PL_inplace		
#undef  PL_last_proto		
#define PL_last_proto				pPerl->PL_last_proto		
#undef  PL_lastfd		
#define PL_lastfd				pPerl->PL_lastfd		
#undef  PL_lastsize		
#define PL_lastsize				pPerl->PL_lastsize		
#undef  PL_lastspbase		
#define PL_lastspbase				pPerl->PL_lastspbase		
#undef  PL_laststatval		
#define PL_laststatval			pPerl->PL_laststatval		
#undef  PL_laststype		
#define PL_laststype				pPerl->PL_laststype		
#undef  PL_leftgv		
#define PL_leftgv				pPerl->PL_leftgv		
#undef  PL_lineary		
#define PL_lineary				pPerl->PL_lineary		
#undef  PL_linestart		
#define PL_linestart				pPerl->PL_linestart		
#undef  PL_localpatches		
#define PL_localpatches			pPerl->PL_localpatches		
#undef  PL_main_cv		
#define PL_main_cv				pPerl->PL_main_cv		
#undef  PL_main_root		
#define PL_main_root				pPerl->PL_main_root		
#undef  PL_main_start		
#define PL_main_start				pPerl->PL_main_start		
#undef  PL_maxsysfd		
#define PL_maxsysfd				pPerl->PL_maxsysfd		
#undef  PL_mess_sv		
#define PL_mess_sv				pPerl->PL_mess_sv		
#undef  PL_minus_F		
#define PL_minus_F				pPerl->PL_minus_F		
#undef  PL_minus_a		
#define PL_minus_a				pPerl->PL_minus_a		
#undef  PL_minus_c		
#define PL_minus_c				pPerl->PL_minus_c		
#undef  PL_minus_l		
#define PL_minus_l				pPerl->PL_minus_l		
#undef  PL_minus_n		
#define PL_minus_n				pPerl->PL_minus_n		
#undef  PL_minus_p		
#define PL_minus_p				pPerl->PL_minus_p		
#undef  PL_modglobal		
#define PL_modglobal				pPerl->PL_modglobal		
#undef  PL_multiline		
#define PL_multiline				pPerl->PL_multiline		
#undef  PL_mystrk		
#define PL_mystrk				pPerl->PL_mystrk		
#undef  PL_ofmt			
#define PL_ofmt					pPerl->PL_ofmt			
#undef  PL_oldlastpm		
#define PL_oldlastpm				pPerl->PL_oldlastpm		
#undef  PL_oldname		
#define PL_oldname				pPerl->PL_oldname		
#undef  PL_op_mask		
#define PL_op_mask				pPerl->PL_op_mask		
#undef  PL_origargc		
#define PL_origargc				pPerl->PL_origargc		
#undef  PL_origargv		
#define PL_origargv				pPerl->PL_origargv		
#undef  PL_origfilename		
#define PL_origfilename			pPerl->PL_origfilename		
#undef  PL_ors			
#define PL_ors					pPerl->PL_ors			
#undef  PL_orslen		
#define PL_orslen				pPerl->PL_orslen		
#undef  PL_parsehook		
#define PL_parsehook				pPerl->PL_parsehook		
#undef  PL_patchlevel		
#define PL_patchlevel				pPerl->PL_patchlevel		
#undef  PL_pending_ident	
#define PL_pending_ident		pPerl->PL_pending_ident	
#undef  PL_perl_destruct_level	
#define PL_perl_destruct_level		pPerl->PL_perl_destruct_level	
#undef  PL_perldb		
#define PL_perldb				pPerl->PL_perldb		
#undef  PL_preambleav		
#define PL_preambleav				pPerl->PL_preambleav		
#undef  PL_preambled		
#define PL_preambled				pPerl->PL_preambled		
#undef  PL_preprocess		
#define PL_preprocess				pPerl->PL_preprocess		
#undef  PL_profiledata		
#define PL_profiledata			pPerl->PL_profiledata		
#undef  PL_replgv		
#define PL_replgv				pPerl->PL_replgv		
#undef  PL_rightgv		
#define PL_rightgv				pPerl->PL_rightgv		
#undef  PL_rsfp			
#define PL_rsfp					pPerl->PL_rsfp			
#undef  PL_rsfp_filters		
#define PL_rsfp_filters			pPerl->PL_rsfp_filters		
#undef  PL_sawampersand		
#define PL_sawampersand			pPerl->PL_sawampersand		
#undef  PL_sawstudy		
#define PL_sawstudy				pPerl->PL_sawstudy		
#undef  PL_sawvec		
#define PL_sawvec				pPerl->PL_sawvec		
#undef  PL_siggv		
#define PL_siggv				pPerl->PL_siggv		
#undef  PL_splitstr		
#define PL_splitstr				pPerl->PL_splitstr		
#undef  PL_statusvalue		
#define PL_statusvalue			pPerl->PL_statusvalue		
#undef  PL_statusvalue_vms	
#define PL_statusvalue_vms		pPerl->PL_statusvalue_vms	
#undef  PL_stdingv		
#define PL_stdingv				pPerl->PL_stdingv		
#undef  PL_strchop		
#define PL_strchop				pPerl->PL_strchop		
#undef  PL_strtab		
#define PL_strtab				pPerl->PL_strtab		
#undef  PL_strtab_mutex
#define PL_strtab_mutex				pPerl->PL_strtab_mutex
#undef  PL_sub_generation	
#define PL_sub_generation		pPerl->PL_sub_generation	
#undef  PL_sublex_info		
#define PL_sublex_info			pPerl->PL_sublex_info		
#undef  PL_sv_arenaroot		
#define PL_sv_arenaroot			pPerl->PL_sv_arenaroot		
#undef  PL_sv_count		
#define PL_sv_count				pPerl->PL_sv_count		
#undef  PL_sv_objcount		
#define PL_sv_objcount			pPerl->PL_sv_objcount		
#undef  PL_sv_root		
#define PL_sv_root				pPerl->PL_sv_root		
#undef  PL_sys_intern		
#define PL_sys_intern				pPerl->PL_sys_intern		
#undef  PL_tainting		
#define PL_tainting				pPerl->PL_tainting		
#undef  PL_threadnum		
#define PL_threadnum				pPerl->PL_threadnum		
#undef  PL_thrsv		
#define PL_thrsv				pPerl->PL_thrsv		
#undef  PL_unsafe		
#define PL_unsafe				pPerl->PL_unsafe		
#undef  PL_warnhook		
#define PL_warnhook				pPerl->PL_warnhook		
#undef  PL_No			
#define PL_No					pPerl->PL_No			
#undef  PL_Yes			
#define PL_Yes					pPerl->PL_Yes			
#undef  PL_amagic_generation	
#define PL_amagic_generation		pPerl->PL_amagic_generation	
#undef  PL_an			
#define PL_an					pPerl->PL_an			
#undef  PL_bufend		
#define PL_bufend				pPerl->PL_bufend		
#undef  PL_bufptr		
#define PL_bufptr				pPerl->PL_bufptr		
#undef  PL_collation_ix		
#define PL_collation_ix			pPerl->PL_collation_ix		
#undef  PL_collation_name	
#define PL_collation_name		pPerl->PL_collation_name	
#undef  PL_collation_standard	
#define PL_collation_standard		pPerl->PL_collation_standard	
#undef  PL_collxfrm_base	
#define PL_collxfrm_base		pPerl->PL_collxfrm_base	
#undef  PL_collxfrm_mult	
#define PL_collxfrm_mult		pPerl->PL_collxfrm_mult	
#undef  PL_cop_seqmax		
#define PL_cop_seqmax				pPerl->PL_cop_seqmax		
#undef  PL_cryptseen		
#define PL_cryptseen				pPerl->PL_cryptseen		
#undef  PL_cshlen		
#define PL_cshlen				pPerl->PL_cshlen		
#undef  PL_cshname		
#define PL_cshname				pPerl->PL_cshname		
#undef  PL_curinterp		
#define PL_curinterp				pPerl->PL_curinterp		
#undef  PL_curthr		
#define PL_curthr				pPerl->PL_curthr		
#undef  PL_debug		
#define PL_debug				pPerl->PL_debug		
#undef  PL_do_undump		
#define PL_do_undump				pPerl->PL_do_undump		
#undef  PL_egid			
#define PL_egid					pPerl->PL_egid			
#undef  PL_error_count		
#define PL_error_count			pPerl->PL_error_count		
#undef  PL_euid			
#define PL_euid					pPerl->PL_euid			
#undef  PL_eval_cond		
#define PL_eval_cond				pPerl->PL_eval_cond		
#undef  PL_eval_mutex		
#define PL_eval_mutex				pPerl->PL_eval_mutex		
#undef  PL_eval_owner		
#define PL_eval_owner				pPerl->PL_eval_owner		
#undef  PL_evalseq		
#define PL_evalseq				pPerl->PL_evalseq		
#undef  PL_expect		
#define PL_expect				pPerl->PL_expect		
#undef  PL_gid			
#define PL_gid					pPerl->PL_gid			
#undef  PL_he_root		
#define PL_he_root				pPerl->PL_he_root		
#undef  PL_hexdigit		
#define PL_hexdigit				pPerl->PL_hexdigit		
#undef  PL_hints		
#define PL_hints				pPerl->PL_hints		
#undef  PL_in_my		
#define PL_in_my				pPerl->PL_in_my		
#undef  PL_in_my_stash		
#define PL_in_my_stash			pPerl->PL_in_my_stash		
#undef  PL_last_lop		
#define PL_last_lop				pPerl->PL_last_lop		
#undef  PL_last_lop_op		
#define PL_last_lop_op			pPerl->PL_last_lop_op		
#undef  PL_last_uni		
#define PL_last_uni				pPerl->PL_last_uni		
#undef  PL_lex_brackets		
#define PL_lex_brackets			pPerl->PL_lex_brackets		
#undef  PL_lex_brackstack	
#define PL_lex_brackstack		pPerl->PL_lex_brackstack	
#undef  PL_lex_casemods		
#define PL_lex_casemods			pPerl->PL_lex_casemods		
#undef  PL_lex_casestack	
#define PL_lex_casestack		pPerl->PL_lex_casestack	
#undef  PL_lex_defer		
#define PL_lex_defer				pPerl->PL_lex_defer		
#undef  PL_lex_dojoin		
#define PL_lex_dojoin				pPerl->PL_lex_dojoin		
#undef  PL_lex_expect		
#define PL_lex_expect				pPerl->PL_lex_expect		
#undef  PL_lex_fakebrack	
#define PL_lex_fakebrack		pPerl->PL_lex_fakebrack	
#undef  PL_lex_formbrack	
#define PL_lex_formbrack		pPerl->PL_lex_formbrack	
#undef  PL_lex_inpat		
#define PL_lex_inpat				pPerl->PL_lex_inpat		
#undef  PL_lex_inwhat		
#define PL_lex_inwhat				pPerl->PL_lex_inwhat		
#undef  PL_lex_op		
#define PL_lex_op				pPerl->PL_lex_op		
#undef  PL_lex_repl		
#define PL_lex_repl				pPerl->PL_lex_repl		
#undef  PL_lex_starts		
#define PL_lex_starts				pPerl->PL_lex_starts		
#undef  PL_lex_state		
#define PL_lex_state				pPerl->PL_lex_state		
#undef  PL_lex_stuff		
#define PL_lex_stuff				pPerl->PL_lex_stuff		
#undef  PL_linestr		
#define PL_linestr				pPerl->PL_linestr		
#undef  PL_malloc_mutex		
#define PL_malloc_mutex			pPerl->PL_malloc_mutex		
#undef  PL_max_intro_pending	
#define PL_max_intro_pending		pPerl->PL_max_intro_pending	
#undef  PL_maxo			
#define PL_maxo					pPerl->PL_maxo			
#undef  PL_min_intro_pending	
#define PL_min_intro_pending		pPerl->PL_min_intro_pending	
#undef  PL_multi_close		
#define PL_multi_close			pPerl->PL_multi_close		
#undef  PL_multi_end		
#define PL_multi_end				pPerl->PL_multi_end		
#undef  PL_multi_open		
#define PL_multi_open				pPerl->PL_multi_open		
#undef  PL_multi_start		
#define PL_multi_start			pPerl->PL_multi_start		
#undef  PL_na			
#define PL_na					pPerl->PL_na			
#undef  PL_nexttoke		
#define PL_nexttoke				pPerl->PL_nexttoke		
#undef  PL_nexttype		
#define PL_nexttype				pPerl->PL_nexttype		
#undef  PL_nextval		
#define PL_nextval				pPerl->PL_nextval		
#undef  PL_nice_chunk		
#define PL_nice_chunk				pPerl->PL_nice_chunk		
#undef  PL_nice_chunk_size	
#define PL_nice_chunk_size		pPerl->PL_nice_chunk_size	
#undef  PL_ninterps		
#define PL_ninterps				pPerl->PL_ninterps		
#undef  PL_nomemok		
#define PL_nomemok				pPerl->PL_nomemok		
#undef  PL_nthreads		
#define PL_nthreads				pPerl->PL_nthreads		
#undef  PL_nthreads_cond	
#define PL_nthreads_cond		pPerl->PL_nthreads_cond	
#undef  PL_numeric_local	
#define PL_numeric_local		pPerl->PL_numeric_local	
#undef  PL_numeric_name		
#define PL_numeric_name			pPerl->PL_numeric_name		
#undef  PL_numeric_standard	
#define PL_numeric_standard		pPerl->PL_numeric_standard	
#undef  PL_oldbufptr		
#define PL_oldbufptr				pPerl->PL_oldbufptr		
#undef  PL_oldoldbufptr		
#define PL_oldoldbufptr			pPerl->PL_oldoldbufptr		
#undef  PL_op_seqmax		
#define PL_op_seqmax				pPerl->PL_op_seqmax		
#undef  PL_origalen		
#define PL_origalen				pPerl->PL_origalen		
#undef  PL_origenviron		
#define PL_origenviron			pPerl->PL_origenviron		
#undef  PL_osname		
#define PL_osname				pPerl->PL_osname		
#undef  PL_pad_reset_pending	
#define PL_pad_reset_pending		pPerl->PL_pad_reset_pending	
#undef  PL_padix		
#define PL_padix				pPerl->PL_padix		
#undef  PL_padix_floor		
#define PL_padix_floor			pPerl->PL_padix_floor		
#undef  PL_patleave		
#define PL_patleave				pPerl->PL_patleave		
#undef  PL_pidstatus		
#define PL_pidstatus				pPerl->PL_pidstatus		
#undef  PL_runops		
#define PL_runops				pPerl->PL_runops		
#undef  PL_sh_path		
#define PL_sh_path				pPerl->PL_sh_path		
#undef  PL_sighandlerp		
#define PL_sighandlerp			pPerl->PL_sighandlerp		
#undef  PL_specialsv_list	
#define PL_specialsv_list		pPerl->PL_specialsv_list	
#undef  PL_subline		
#define PL_subline				pPerl->PL_subline		
#undef  PL_subname		
#define PL_subname				pPerl->PL_subname		
#undef  PL_sv_mutex		
#define PL_sv_mutex				pPerl->PL_sv_mutex		
#undef  PL_sv_no		
#define PL_sv_no				pPerl->PL_sv_no		
#undef  PL_sv_undef		
#define PL_sv_undef				pPerl->PL_sv_undef		
#undef  PL_sv_yes		
#define PL_sv_yes				pPerl->PL_sv_yes		
#undef  PL_svref_mutex		
#define PL_svref_mutex			pPerl->PL_svref_mutex		
#undef  PL_thisexpr		
#define PL_thisexpr				pPerl->PL_thisexpr		
#undef  PL_thr_key		
#define PL_thr_key				pPerl->PL_thr_key		
#undef  PL_threads_mutex	
#define PL_threads_mutex		pPerl->PL_threads_mutex	
#undef  PL_threadsv_names	
#define PL_threadsv_names		pPerl->PL_threadsv_names	
#undef  PL_tokenbuf		
#define PL_tokenbuf				pPerl->PL_tokenbuf		
#undef  PL_uid			
#define PL_uid					pPerl->PL_uid			
#undef  PL_xiv_arenaroot	
#define PL_xiv_arenaroot		pPerl->PL_xiv_arenaroot	
#undef  PL_xiv_root		
#define PL_xiv_root				pPerl->PL_xiv_root		
#undef  PL_xnv_root		
#define PL_xnv_root				pPerl->PL_xnv_root		
#undef  PL_xpv_root		
#define PL_xpv_root				pPerl->PL_xpv_root		
#undef  PL_xrv_root		
#define PL_xrv_root				pPerl->PL_xrv_root		

/* Functions */

#undef  amagic_call
#define amagic_call         pPerl->Perl_amagic_call
#undef  Perl_GetVars
#define Perl_GetVars        pPerl->Perl_GetVars
#undef  Gv_AMupdate
#define Gv_AMupdate         pPerl->Perl_Gv_AMupdate
#undef  append_elem
#define append_elem         pPerl->Perl_append_elem
#undef  append_list
#define append_list         pPerl->Perl_append_list
#undef  apply
#define apply               pPerl->Perl_apply
#undef  assertref
#define assertref           pPerl->Perl_assertref
#undef  av_clear
#define av_clear            pPerl->Perl_av_clear
#undef  av_extend
#define av_extend           pPerl->Perl_av_extend
#undef  av_fake
#define av_fake             pPerl->Perl_av_fake
#undef  av_fetch
#define av_fetch            pPerl->Perl_av_fetch
#undef  av_fill
#define av_fill             pPerl->Perl_av_fill
#undef  av_len
#define av_len              pPerl->Perl_av_len
#undef  av_make
#define av_make             pPerl->Perl_av_make
#undef  av_pop
#define av_pop              pPerl->Perl_av_pop
#undef  av_push
#define av_push             pPerl->Perl_av_push
#undef  av_reify
#define av_reify            pPerl->Perl_av_reify
#undef  av_shift
#define av_shift            pPerl->Perl_av_shift
#undef  av_store
#define av_store            pPerl->Perl_av_store
#undef  av_undef
#define av_undef            pPerl->Perl_av_undef
#undef  av_unshift
#define av_unshift          pPerl->Perl_av_unshift
#undef  avhv_exists_ent
#define avhv_exists_ent     pPerl->Perl_avhv_exists_ent
#undef  avhv_fetch_ent
#define avhv_fetch_ent      pPerl->Perl_avhv_fetch_ent
#undef  avhv_iternext
#define avhv_iternext       pPerl->Perl_avhv_iternext
#undef  avhv_iterval
#define avhv_iterval        pPerl->Perl_avhv_iterval
#undef  avhv_keys
#define avhv_keys           pPerl->Perl_avhv_keys
#undef  bind_match
#define bind_match          pPerl->Perl_bind_match
#undef  block_end
#define block_end           pPerl->Perl_block_end
#undef  block_gimme
#define block_gimme         pPerl->Perl_block_gimme
#undef  block_start
#define block_start         pPerl->Perl_block_start
#undef  byterun
#define byterun             pPerl->Perl_byterun
#undef  call_list
#define call_list           pPerl->Perl_call_list
#undef  cando
#define cando               pPerl->Perl_cando
#undef  cast_ulong
#define cast_ulong          pPerl->Perl_cast_ulong
#undef  checkcomma
#define checkcomma          pPerl->Perl_checkcomma
#undef  check_uni
#define check_uni           pPerl->Perl_check_uni
#undef  ck_concat
#define ck_concat           pPerl->Perl_ck_concat
#undef  ck_delete
#define ck_delete           pPerl->Perl_ck_delete
#undef  ck_eof
#define ck_eof              pPerl->Perl_ck_eof
#undef  ck_eval
#define ck_eval             pPerl->Perl_ck_eval
#undef  ck_exec
#define ck_exec             pPerl->Perl_ck_exec
#undef  ck_formline
#define ck_formline         pPerl->Perl_ck_formline
#undef  ck_ftst
#define ck_ftst             pPerl->Perl_ck_ftst
#undef  ck_fun
#define ck_fun              pPerl->Perl_ck_fun
#undef  ck_glob
#define ck_glob             pPerl->Perl_ck_glob
#undef  ck_grep
#define ck_grep             pPerl->Perl_ck_grep
#undef  ck_gvconst
#define ck_gvconst          pPerl->Perl_ck_gvconst
#undef  ck_index
#define ck_index            pPerl->Perl_ck_index
#undef  ck_lengthconst
#define ck_lengthconst      pPerl->Perl_ck_lengthconst
#undef  ck_lfun
#define ck_lfun             pPerl->Perl_ck_lfun
#undef  ck_listiob
#define ck_listiob          pPerl->Perl_ck_listiob
#undef  ck_match
#define ck_match            pPerl->Perl_ck_match
#undef  ck_null
#define ck_null             pPerl->Perl_ck_null
#undef  ck_repeat
#define ck_repeat           pPerl->Perl_ck_repeat
#undef  ck_require
#define ck_require          pPerl->Perl_ck_require
#undef  ck_retarget
#define ck_retarget         pPerl->Perl_ck_retarget
#undef  ck_rfun
#define ck_rfun             pPerl->Perl_ck_rfun
#undef  ck_rvconst
#define ck_rvconst          pPerl->Perl_ck_rvconst
#undef  ck_select
#define ck_select           pPerl->Perl_ck_select
#undef  ck_shift
#define ck_shift            pPerl->Perl_ck_shift
#undef  ck_sort
#define ck_sort             pPerl->Perl_ck_sort
#undef  ck_spair
#define ck_spair            pPerl->Perl_ck_spair
#undef  ck_split
#define ck_split            pPerl->Perl_ck_split
#undef  ck_subr
#define ck_subr             pPerl->Perl_ck_subr
#undef  ck_svconst
#define ck_svconst          pPerl->Perl_ck_svconst
#undef  ck_trunc
#define ck_trunc            pPerl->Perl_ck_trunc
#undef  condpair_magic
#define condpair_magic      pPerl->Perl_condpair_magic
#undef  convert
#define convert             pPerl->Perl_convert
#undef  cpytill
#define cpytill             pPerl->Perl_cpytill
#undef  croak
#define croak               pPerl->Perl_croak
#undef  cv_ckproto
#define cv_ckproto          pPerl->Perl_cv_ckproto
#undef  cv_clone
#define cv_clone            pPerl->Perl_cv_clone
#undef  cv_const_sv
#define cv_const_sv         pPerl->Perl_cv_const_sv
#undef  cv_undef
#define cv_undef            pPerl->Perl_cv_undef
#undef  cx_dump
#define cx_dump             pPerl->Perl_cx_dump
#undef  cxinc
#define cxinc               pPerl->Perl_cxinc
#undef  deb
#define deb                 pPerl->Perl_deb
#undef  deb_growlevel
#define deb_growlevel       pPerl->Perl_deb_growlevel
#undef  debprofdump
#define debprofdump         pPerl->Perl_debprofdump
#undef  debop
#define debop               pPerl->Perl_debop
#undef  debstack
#define debstack            pPerl->Perl_debstack
#undef  debstackptrs
#define debstackptrs        pPerl->Perl_debstackptrs
#undef  delimcpy
#define delimcpy            pPerl->Perl_delimcpy
#undef  deprecate
#define deprecate           pPerl->Perl_deprecate
#undef  die
#define die                 pPerl->Perl_die
#undef  die_where
#define die_where           pPerl->Perl_die_where
#undef  dopoptoeval
#define dopoptoeval         pPerl->Perl_dopoptoeval
#undef  dounwind
#define dounwind            pPerl->Perl_dounwind
#undef  do_aexec
#define do_aexec            pPerl->Perl_do_aexec
#undef  do_binmode
#define do_binmode          pPerl->Perl_do_binmode
#undef  do_chomp
#define do_chomp            pPerl->Perl_do_chomp
#undef  do_chop
#define do_chop             pPerl->Perl_do_chop
#undef  do_close
#define do_close            pPerl->Perl_do_close
#undef  do_eof
#define do_eof              pPerl->Perl_do_eof
#undef  do_exec
#define do_exec             pPerl->Perl_do_exec
#undef  do_execfree
#define do_execfree         pPerl->Perl_do_execfree
#undef  do_join
#define do_join             pPerl->Perl_do_join
#undef  do_kv
#define do_kv               pPerl->Perl_do_kv
#undef  do_open
#define do_open             pPerl->Perl_do_open
#undef  do_pipe
#define do_pipe             pPerl->Perl_do_pipe
#undef  do_print
#define do_print            pPerl->Perl_do_print
#undef  do_readline
#define do_readline         pPerl->Perl_do_readline
#undef  do_seek
#define do_seek             pPerl->Perl_do_seek
#undef  do_sprintf
#define do_sprintf          pPerl->Perl_do_sprintf
#undef  do_sysseek
#define do_sysseek          pPerl->Perl_do_sysseek
#undef  do_tell
#define do_tell             pPerl->Perl_do_tell
#undef  do_trans
#define do_trans            pPerl->Perl_do_trans
#undef  do_vecset
#define do_vecset           pPerl->Perl_do_vecset
#undef  do_vop
#define do_vop              pPerl->Perl_do_vop
#undef  dofile
#define dofile              pPerl->Perl_dofile
#undef  dowantarray
#define dowantarray         pPerl->Perl_dowantarray
#undef  dump_all
#define dump_all            pPerl->Perl_dump_all
#undef  dump_eval
#define dump_eval           pPerl->Perl_dump_eval
#undef  dump_fds
#define dump_fds            pPerl->Perl_dump_fds
#undef  dump_form
#define dump_form           pPerl->Perl_dump_form
#undef  dump_gv
#define dump_gv             pPerl->Perl_dump_gv
#undef  dump_mstats
#define dump_mstats         pPerl->Perl_dump_mstats
#undef  dump_op
#define dump_op             pPerl->Perl_dump_op
#undef  dump_pm
#define dump_pm             pPerl->Perl_dump_pm
#undef  dump_packsubs
#define dump_packsubs       pPerl->Perl_dump_packsubs
#undef  dump_sub
#define dump_sub            pPerl->Perl_dump_sub
#undef  fbm_compile
#define fbm_compile         pPerl->Perl_fbm_compile
#undef  fbm_instr
#define fbm_instr           pPerl->Perl_fbm_instr
#undef  filter_add
#define filter_add          pPerl->Perl_filter_add
#undef  filter_del
#define filter_del          pPerl->Perl_filter_del
#undef  filter_read
#define filter_read         pPerl->Perl_filter_read
#undef  find_threadsv
#define find_threadsv       pPerl->Perl_find_threadsv
#undef  find_script
#define find_script         pPerl->Perl_find_script
#undef  force_ident
#define force_ident         pPerl->Perl_force_ident
#undef  force_list
#define force_list          pPerl->Perl_force_list
#undef  force_next
#define force_next          pPerl->Perl_force_next
#undef  force_word
#define force_word          pPerl->Perl_force_word
#undef  form
#define form                pPerl->Perl_form
#undef  fold_constants
#define fold_constants      pPerl->Perl_fold_constants
#undef  fprintf
#define fprintf             pPerl->fprintf
#undef  free_tmps
#define free_tmps           pPerl->Perl_free_tmps
#undef  gen_constant_list
#define gen_constant_list   pPerl->Perl_gen_constant_list
#undef  get_op_descs
#define get_op_descs        pPerl->Perl_get_op_descs
#undef  get_op_names
#define get_op_names        pPerl->Perl_get_op_names
#undef  get_no_modify
#define get_no_modify       pPerl->Perl_get_no_modify
#undef  get_opargs
#define get_opargs	        pPerl->Perl_get_opargs
#undef  get_specialsv_list
#define get_specialsv_list  pPerl->Perl_get_specialsv_list
#undef  get_vtbl
#define get_vtbl            pPerl->Perl_get_vtbl
#undef  gp_free
#define gp_free             pPerl->Perl_gp_free
#undef  gp_ref
#define gp_ref              pPerl->Perl_gp_ref
#undef  gv_AVadd
#define gv_AVadd            pPerl->Perl_gv_AVadd
#undef  gv_HVadd
#define gv_HVadd            pPerl->Perl_gv_HVadd
#undef  gv_IOadd
#define gv_IOadd            pPerl->Perl_gv_IOadd
#undef  gv_autoload4
#define gv_autoload4        pPerl->Perl_gv_autoload4
#undef  gv_check
#define gv_check            pPerl->Perl_gv_check
#undef  gv_efullname
#define gv_efullname        pPerl->Perl_gv_efullname
#undef  gv_efullname3
#define gv_efullname3       pPerl->Perl_gv_efullname3
#undef  gv_fetchfile
#define gv_fetchfile        pPerl->Perl_gv_fetchfile
#undef  gv_fetchmeth
#define gv_fetchmeth        pPerl->Perl_gv_fetchmeth
#undef  gv_fetchmethod
#define gv_fetchmethod      pPerl->Perl_gv_fetchmethod
#undef  gv_fetchmethod_autoload
#define gv_fetchmethod_autoload pPerl->Perl_gv_fetchmethod_autoload
#undef  gv_fetchpv
#define gv_fetchpv          pPerl->Perl_gv_fetchpv
#undef  gv_fullname
#define gv_fullname         pPerl->Perl_gv_fullname
#undef  gv_fullname3
#define gv_fullname3        pPerl->Perl_gv_fullname3
#undef  gv_init
#define gv_init             pPerl->Perl_gv_init
#undef  gv_stashpv
#define gv_stashpv          pPerl->Perl_gv_stashpv
#undef  gv_stashpvn
#define gv_stashpvn         pPerl->Perl_gv_stashpvn
#undef  gv_stashsv
#define gv_stashsv          pPerl->Perl_gv_stashsv
#undef  he_delayfree
#define he_delayfree        pPerl->Perl_he_delayfree
#undef  he_free
#define he_free             pPerl->Perl_he_free
#undef  hoistmust
#define hoistmust           pPerl->Perl_hoistmust
#undef  hv_clear
#define hv_clear            pPerl->Perl_hv_clear
#undef  hv_delayfree_ent
#define hv_delayfree_ent    pPerl->Perl_hv_delayfree_ent
#undef  hv_delete
#define hv_delete           pPerl->Perl_hv_delete
#undef  hv_delete_ent
#define hv_delete_ent       pPerl->Perl_hv_delete_ent
#undef  hv_exists
#define hv_exists           pPerl->Perl_hv_exists
#undef  hv_exists_ent
#define hv_exists_ent       pPerl->Perl_hv_exists_ent
#undef  hv_fetch
#define hv_fetch            pPerl->Perl_hv_fetch
#undef  hv_fetch_ent
#define hv_fetch_ent        pPerl->Perl_hv_fetch_ent
#undef  hv_free_ent
#define hv_free_ent         pPerl->Perl_hv_free_ent
#undef  hv_iterinit
#define hv_iterinit         pPerl->Perl_hv_iterinit
#undef  hv_iterkey
#define hv_iterkey          pPerl->Perl_hv_iterkey
#undef  hv_iterkeysv
#define hv_iterkeysv        pPerl->Perl_hv_iterkeysv
#undef  hv_iternext
#define hv_iternext         pPerl->Perl_hv_iternext
#undef  hv_iternextsv
#define hv_iternextsv       pPerl->Perl_hv_iternextsv
#undef  hv_iterval
#define hv_iterval          pPerl->Perl_hv_iterval
#undef  hv_ksplit
#define hv_ksplit           pPerl->Perl_hv_ksplit
#undef  hv_magic
#define hv_magic            pPerl->Perl_hv_magic
#undef  hv_store
#define hv_store            pPerl->Perl_hv_store
#undef  hv_store_ent
#define hv_store_ent        pPerl->Perl_hv_store_ent
#undef  hv_undef
#define hv_undef            pPerl->Perl_hv_undef
#undef  ibcmp
#define ibcmp               pPerl->Perl_ibcmp
#undef  ibcmp_locale
#define ibcmp_locale        pPerl->Perl_ibcmp_locale
#undef  incpush
#define incpush             pPerl->incpush
#undef  incline
#define incline             pPerl->incline
#undef  incl_perldb
#define incl_perldb         pPerl->incl_perldb
#undef  ingroup
#define ingroup             pPerl->Perl_ingroup
#undef  init_stacks
#define init_stacks         pPerl->Perl_init_stacks
#undef  instr
#define instr               pPerl->Perl_instr
#undef  intro_my
#define intro_my            pPerl->Perl_intro_my
#undef  intuit_method
#define intuit_method       pPerl->intuit_method
#undef  intuit_more
#define intuit_more         pPerl->Perl_intuit_more
#undef  invert
#define invert              pPerl->Perl_invert
#undef  io_close
#define io_close            pPerl->Perl_io_close
#undef  ioctl
#define ioctl               pPerl->ioctl
#undef  jmaybe
#define jmaybe              pPerl->Perl_jmaybe
#undef  keyword
#define keyword             pPerl->Perl_keyword
#undef  leave_scope
#define leave_scope         pPerl->Perl_leave_scope
#undef  lex_end
#define lex_end             pPerl->Perl_lex_end
#undef  lex_start
#define lex_start           pPerl->Perl_lex_start
#undef  linklist
#define linklist            pPerl->Perl_linklist
#undef  list
#define list                pPerl->Perl_list
#undef  listkids
#define listkids            pPerl->Perl_listkids
#undef  lop
#define lop                 pPerl->lop
#undef  localize
#define localize            pPerl->Perl_localize
#undef  looks_like_number
#define looks_like_number   pPerl->Perl_looks_like_number
#undef  magic_clear_all_env
#define magic_clear_all_env pPerl->Perl_magic_clear_all_env
#undef  magic_clearenv
#define magic_clearenv      pPerl->Perl_magic_clearenv
#undef  magic_clearpack
#define magic_clearpack     pPerl->Perl_magic_clearpack
#undef  magic_clearsig
#define magic_clearsig      pPerl->Perl_magic_clearsig
#undef  magic_existspack
#define magic_existspack    pPerl->Perl_magic_existspack
#undef  magic_freeregexp
#define magic_freeregexp    pPerl->Perl_magic_freeregexp
#undef  magic_get
#define magic_get           pPerl->Perl_magic_get
#undef  magic_getarylen
#define magic_getarylen     pPerl->Perl_magic_getarylen
#undef  magic_getdefelem
#define magic_getdefelem    pPerl->Perl_magic_getdefelem
#undef  magic_getpack
#define magic_getpack       pPerl->Perl_magic_getpack
#undef  magic_getglob
#define magic_getglob       pPerl->Perl_magic_getglob
#undef  magic_getnkeys
#define magic_getnkeys      pPerl->Perl_magic_getnkeys
#undef  magic_getpos
#define magic_getpos        pPerl->Perl_magic_getpos
#undef  magic_getsig
#define magic_getsig        pPerl->Perl_magic_getsig
#undef  magic_getsubstr
#define magic_getsubstr     pPerl->Perl_magic_getsubstr
#undef  magic_gettaint
#define magic_gettaint      pPerl->Perl_magic_gettaint
#undef  magic_getuvar
#define magic_getuvar       pPerl->Perl_magic_getuvar
#undef  magic_getvec
#define magic_getvec        pPerl->Perl_magic_getvec
#undef  magic_len
#define magic_len           pPerl->Perl_magic_len
#undef  magic_methpack
#define magic_methpack      pPerl->magic_methpack
#undef  magic_mutexfree
#define magic_mutexfree     pPerl->Perl_magic_mutexfree
#undef  magic_nextpack
#define magic_nextpack      pPerl->Perl_magic_nextpack
#undef  magic_set
#define magic_set           pPerl->Perl_magic_set
#undef  magic_set_all_env
#define magic_set_all_env   pPerl->Perl_magic_set_all_env
#undef  magic_setamagic
#define magic_setamagic     pPerl->Perl_magic_setamagic
#undef  magic_setarylen
#define magic_setarylen     pPerl->Perl_magic_setarylen
#undef  magic_setbm
#define magic_setbm         pPerl->Perl_magic_setbm
#undef  magic_setcollxfrm
#define magic_setcollxfrm   pPerl->Perl_magic_setcollxfrm
#undef  magic_setdbline
#define magic_setdbline     pPerl->Perl_magic_setdbline
#undef  magic_setdefelem
#define magic_setdefelem    pPerl->Perl_magic_setdefelem
#undef  magic_setenv
#define magic_setenv        pPerl->Perl_magic_setenv
#undef  magic_setfm
#define magic_setfm         pPerl->Perl_magic_setfm
#undef  magic_setisa
#define magic_setisa        pPerl->Perl_magic_setisa
#undef  magic_setglob
#define magic_setglob       pPerl->Perl_magic_setglob
#undef  magic_setmglob
#define magic_setmglob      pPerl->Perl_magic_setmglob
#undef  magic_setnkeys
#define magic_setnkeys      pPerl->Perl_magic_setnkeys
#undef  magic_setpack
#define magic_setpack       pPerl->Perl_magic_setpack
#undef  magic_setpos
#define magic_setpos        pPerl->Perl_magic_setpos
#undef  magic_setsig
#define magic_setsig        pPerl->Perl_magic_setsig
#undef  magic_setsubstr
#define magic_setsubstr     pPerl->Perl_magic_setsubstr
#undef  magic_settaint
#define magic_settaint      pPerl->Perl_magic_settaint
#undef  magic_setuvar
#define magic_setuvar       pPerl->Perl_magic_setuvar
#undef  magic_setvec
#define magic_setvec        pPerl->Perl_magic_setvec
#undef  magic_sizepack
#define magic_sizepack      pPerl->Perl_magic_sizepack
#undef  magic_unchain
#define magic_unchain       pPerl->Perl_magic_unchain
#undef  magic_wipepack
#define magic_wipepack      pPerl->Perl_magic_wipepack
#undef  magicname
#define magicname           pPerl->Perl_magicname
#undef  malloced_size
#define malloced_size       pPerl->Perl_malloced_size
#undef  markstack_grow
#define markstack_grow      pPerl->Perl_markstack_grow
#undef  mem_collxfrm
#define mem_collxfrm        pPerl->Perl_mem_collxfrm
#undef  mess
#define mess                pPerl->Perl_mess
#undef  mg_clear
#define mg_clear            pPerl->Perl_mg_clear
#undef  mg_copy
#define mg_copy             pPerl->Perl_mg_copy
#undef  mg_find
#define mg_find             pPerl->Perl_mg_find
#undef  mg_free
#define mg_free             pPerl->Perl_mg_free
#undef  mg_get
#define mg_get              pPerl->Perl_mg_get
#undef  mg_magical
#define mg_magical          pPerl->Perl_mg_magical
#undef  mg_length
#define mg_length           pPerl->Perl_mg_length
#undef  mg_set
#define mg_set              pPerl->Perl_mg_set
#undef  mg_size
#define mg_size             pPerl->Perl_mg_size
#undef  missingterm
#define missingterm         pPerl->missingterm
#undef  mod
#define mod                 pPerl->Perl_mod
#undef  modkids
#define modkids             pPerl->Perl_modkids
#undef  moreswitches
#define moreswitches        pPerl->Perl_moreswitches
#undef  more_sv
#define more_sv             pPerl->more_sv
#undef  more_xiv
#define more_xiv            pPerl->more_xiv
#undef  more_xnv
#define more_xnv            pPerl->more_xnv
#undef  more_xpv
#define more_xpv            pPerl->more_xpv
#undef  more_xrv
#define more_xrv            pPerl->more_xrv
#undef  my
#define my                  pPerl->Perl_my
#undef  my_bcopy
#define my_bcopy            pPerl->Perl_my_bcopy
#undef  my_bzero
#define my_bzero            pPerl->Perl_my_bzero
#undef  my_chsize
#define my_chsize           pPerl->Perl_my_chsize
#undef  my_exit
#define my_exit             pPerl->Perl_my_exit
#undef  my_failure_exit
#define my_failure_exit     pPerl->Perl_my_failure_exit
#undef  my_htonl
#define my_htonl            pPerl->Perl_my_htonl
#undef  my_lstat
#define my_lstat            pPerl->Perl_my_lstat
#undef  my_memcmp
#define my_memcmp           pPerl->my_memcmp
#undef  my_ntohl
#define my_ntohl            pPerl->Perl_my_ntohl
#undef  my_pclose
#define my_pclose           pPerl->Perl_my_pclose
#undef  my_popen
#define my_popen            pPerl->Perl_my_popen
#undef  my_setenv
#define my_setenv           pPerl->Perl_my_setenv
#undef  my_stat
#define my_stat             pPerl->Perl_my_stat
#undef  my_swap
#define my_swap             pPerl->Perl_my_swap
#undef  my_unexec
#define my_unexec           pPerl->Perl_my_unexec
#undef  newANONLIST
#define newANONLIST         pPerl->Perl_newANONLIST
#undef  newANONHASH
#define newANONHASH         pPerl->Perl_newANONHASH
#undef  newANONSUB
#define newANONSUB          pPerl->Perl_newANONSUB
#undef  newASSIGNOP
#define newASSIGNOP         pPerl->Perl_newASSIGNOP
#undef  newCONDOP
#define newCONDOP           pPerl->Perl_newCONDOP
#undef  newCONSTSUB
#define newCONSTSUB         pPerl->Perl_newCONSTSUB
#undef  newFORM
#define newFORM             pPerl->Perl_newFORM
#undef  newFOROP
#define newFOROP            pPerl->Perl_newFOROP
#undef  newLOGOP
#define newLOGOP            pPerl->Perl_newLOGOP
#undef  newLOOPEX
#define newLOOPEX           pPerl->Perl_newLOOPEX
#undef  newLOOPOP
#define newLOOPOP           pPerl->Perl_newLOOPOP
#undef  newMETHOD
#define newMETHOD           pPerl->Perl_newMETHOD
#undef  newNULLLIST
#define newNULLLIST         pPerl->Perl_newNULLLIST
#undef  newOP
#define newOP               pPerl->Perl_newOP
#undef  newPROG
#define newPROG             pPerl->Perl_newPROG
#undef  newRANGE
#define newRANGE            pPerl->Perl_newRANGE
#undef  newSLICEOP
#define newSLICEOP          pPerl->Perl_newSLICEOP
#undef  newSTATEOP
#define newSTATEOP          pPerl->Perl_newSTATEOP
#undef  newSUB
#define newSUB              pPerl->Perl_newSUB
#undef  newXS
#define newXS               pPerl->Perl_newXS
#undef  newAV
#define newAV               pPerl->Perl_newAV
#undef  newAVREF
#define newAVREF            pPerl->Perl_newAVREF
#undef  newBINOP
#define newBINOP            pPerl->Perl_newBINOP
#undef  newCVREF
#define newCVREF            pPerl->Perl_newCVREF
#undef  newCVOP
#define newCVOP             pPerl->Perl_newCVOP
#undef  newGVOP
#define newGVOP             pPerl->Perl_newGVOP
#undef  newGVgen
#define newGVgen            pPerl->Perl_newGVgen
#undef  newGVREF
#define newGVREF            pPerl->Perl_newGVREF
#undef  newHVREF
#define newHVREF            pPerl->Perl_newHVREF
#undef  newHV
#define newHV               pPerl->Perl_newHV
#undef  newHVhv
#define newHVhv             pPerl->Perl_newHVhv
#undef  newIO
#define newIO               pPerl->Perl_newIO
#undef  newLISTOP
#define newLISTOP           pPerl->Perl_newLISTOP
#undef  newPMOP
#define newPMOP             pPerl->Perl_newPMOP
#undef  newPVOP
#define newPVOP             pPerl->Perl_newPVOP
#undef  newRV
#define newRV               pPerl->Perl_newRV
#undef  newRV_noinc
#undef  Perl_newRV_noinc
#define newRV_noinc         pPerl->Perl_newRV_noinc
#undef  newSV
#define newSV               pPerl->Perl_newSV
#undef  newSVREF
#define newSVREF            pPerl->Perl_newSVREF
#undef  newSVOP
#define newSVOP             pPerl->Perl_newSVOP
#undef  newSViv
#define newSViv             pPerl->Perl_newSViv
#undef  newSVnv
#define newSVnv             pPerl->Perl_newSVnv
#undef  newSVpv
#define newSVpv             pPerl->Perl_newSVpv
#undef  newSVpvf
#define newSVpvf            pPerl->Perl_newSVpvf
#undef  newSVpvn
#define newSVpvn            pPerl->Perl_newSVpvn
#undef  newSVrv
#define newSVrv             pPerl->Perl_newSVrv
#undef  newSVsv
#define newSVsv             pPerl->Perl_newSVsv
#undef  newUNOP
#define newUNOP             pPerl->Perl_newUNOP
#undef  newWHILEOP
#define newWHILEOP          pPerl->Perl_newWHILEOP
#undef  new_struct_thread
#define new_struct_thread   pPerl->Perl_new_struct_thread
#undef  new_stackinfo
#define new_stackinfo       pPerl->Perl_new_stackinfo
#undef  new_sv
#define new_sv              pPerl->new_sv
#undef  new_xnv
#define new_xnv             pPerl->new_xnv
#undef  new_xpv
#define new_xpv             pPerl->new_xpv
#undef  nextargv
#define nextargv            pPerl->Perl_nextargv
#undef  nextchar
#define nextchar            pPerl->nextchar
#undef  ninstr
#define ninstr              pPerl->Perl_ninstr
#undef  no_fh_allowed
#define no_fh_allowed       pPerl->Perl_no_fh_allowed
#undef  no_op
#define no_op               pPerl->Perl_no_op
#undef  package
#define package             pPerl->Perl_package
#undef  pad_alloc
#define pad_alloc           pPerl->Perl_pad_alloc
#undef  pad_allocmy
#define pad_allocmy         pPerl->Perl_pad_allocmy
#undef  pad_findmy
#define pad_findmy          pPerl->Perl_pad_findmy
#undef  op_const_sv
#define op_const_sv         pPerl->Perl_op_const_sv
#undef  op_free
#define op_free             pPerl->Perl_op_free
#undef  oopsCV
#define oopsCV              pPerl->Perl_oopsCV
#undef  oopsAV
#define oopsAV              pPerl->Perl_oopsAV
#undef  oopsHV
#define oopsHV              pPerl->Perl_oopsHV
#undef  opendir
#define opendir             pPerl->opendir
#undef  pad_leavemy
#define pad_leavemy         pPerl->Perl_pad_leavemy
#undef  pad_sv
#define pad_sv              pPerl->Perl_pad_sv
#undef  pad_findlex
#define pad_findlex         pPerl->pad_findlex
#undef  pad_free
#define pad_free            pPerl->Perl_pad_free
#undef  pad_reset
#define pad_reset           pPerl->Perl_pad_reset
#undef  pad_swipe
#define pad_swipe           pPerl->Perl_pad_swipe
#undef  peep
#define peep                pPerl->Perl_peep
#undef  perl_atexit
#define perl_atexit         pPerl->perl_atexit
#undef  perl_call_argv
#define perl_call_argv      pPerl->perl_call_argv
#undef  perl_call_method
#define perl_call_method    pPerl->perl_call_method
#undef  perl_call_pv
#define perl_call_pv        pPerl->perl_call_pv
#undef  perl_call_sv
#define perl_call_sv        pPerl->perl_call_sv
#undef  perl_callargv
#define perl_callargv       pPerl->perl_callargv
#undef  perl_callpv
#define perl_callpv         pPerl->perl_callpv
#undef  perl_callsv
#define perl_callsv         pPerl->perl_callsv
#undef  perl_eval_pv
#define perl_eval_pv        pPerl->perl_eval_pv
#undef  perl_eval_sv
#define perl_eval_sv        pPerl->perl_eval_sv
#undef  perl_get_sv
#define perl_get_sv         pPerl->perl_get_sv
#undef  perl_get_av
#define perl_get_av         pPerl->perl_get_av
#undef  perl_get_hv
#define perl_get_hv         pPerl->perl_get_hv
#undef  perl_get_cv
#define perl_get_cv         pPerl->perl_get_cv
#undef  perl_init_i18nl10n
#define perl_init_i18nl10n  pPerl->perl_init_i18nl10n
#undef  perl_init_i18nl14n
#define perl_init_i18nl14n  pPerl->perl_init_i18nl14n
#undef  perl_new_collate
#define perl_new_collate    pPerl->perl_new_collate
#undef  perl_new_ctype
#define perl_new_ctype      pPerl->perl_new_ctype
#undef  perl_new_numeric
#define perl_new_numeric    pPerl->perl_new_numeric
#undef  perl_set_numeric_local
#define perl_set_numeric_local pPerl->perl_set_numeric_local
#undef  perl_set_numeric_standard
#define perl_set_numeric_standard pPerl->perl_set_numeric_standard
#undef  perl_require_pv
#define perl_require_pv     pPerl->perl_require_pv
#undef  pidgone
#define pidgone             pPerl->Perl_pidgone
#undef  pmflag
#define pmflag              pPerl->Perl_pmflag
#undef  pmruntime
#define pmruntime           pPerl->Perl_pmruntime
#undef  pmtrans
#define pmtrans             pPerl->Perl_pmtrans
#undef  pop_return
#define pop_return          pPerl->Perl_pop_return
#undef  pop_scope
#define pop_scope           pPerl->Perl_pop_scope
#undef  prepend_elem
#define prepend_elem        pPerl->Perl_prepend_elem
#undef  push_return
#define push_return         pPerl->Perl_push_return
#undef  push_scope
#define push_scope          pPerl->Perl_push_scope
#undef  pregcomp
#define pregcomp            pPerl->Perl_pregcomp
#undef  ref
#define ref                 pPerl->Perl_ref
#undef  refkids
#define refkids             pPerl->Perl_refkids
#undef  regexec_flags
#define regexec_flags       pPerl->Perl_regexec_flags
#undef  pregexec
#define pregexec            pPerl->Perl_pregexec
#undef  pregfree
#define pregfree            pPerl->Perl_pregfree
#undef  regdump
#define regdump             pPerl->Perl_regdump
#undef  regnext
#define regnext             pPerl->Perl_regnext
#undef  regnoderegnext
#define regnoderegnext      pPerl->regnoderegnext
#undef  regprop
#define regprop             pPerl->Perl_regprop
#undef  repeatcpy
#define repeatcpy           pPerl->Perl_repeatcpy
#undef  rninstr
#define rninstr             pPerl->Perl_rninstr
#undef  rsignal
#define rsignal             pPerl->Perl_rsignal
#undef  rsignal_restore
#define rsignal_restore     pPerl->Perl_rsignal_restore
#undef  rsignal_save
#define rsignal_save        pPerl->Perl_rsignal_save
#undef  rsignal_state
#define rsignal_state       pPerl->Perl_rsignal_state
#undef  run
#define run                 pPerl->Perl_run
#undef  rxres_free
#define rxres_free          pPerl->Perl_rxres_free
#undef  rxres_restore
#define rxres_restore       pPerl->Perl_rxres_restore
#undef  rxres_save
#define rxres_save          pPerl->Perl_rxres_save
#undef  safefree
#define safefree            pPerl->Perl_safefree
#undef  safecalloc
#define safecalloc          pPerl->Perl_safecalloc
#undef  safemalloc
#define safemalloc          pPerl->Perl_safemalloc
#undef  saferealloc
#define saferealloc         pPerl->Perl_saferealloc
#undef  safexcalloc
#define safexcalloc         pPerl->Perl_safexcalloc
#undef  safexfree
#define safexfree           pPerl->Perl_safexfree
#undef  safexmalloc
#define safexmalloc         pPerl->Perl_safexmalloc
#undef  safexrealloc
#define safexrealloc        pPerl->Perl_safexrealloc
#undef  same_dirent
#define same_dirent         pPerl->Perl_same_dirent
#undef  savepv
#define savepv              pPerl->Perl_savepv
#undef  savepvn
#define savepvn             pPerl->Perl_savepvn
#undef  savestack_grow
#define savestack_grow      pPerl->Perl_savestack_grow
#undef  save_aelem
#define save_aelem          pPerl->Perl_save_aelem
#undef  save_aptr
#define save_aptr           pPerl->Perl_save_aptr
#undef  save_ary
#define save_ary            pPerl->Perl_save_ary
#undef  save_clearsv
#define save_clearsv        pPerl->Perl_save_clearsv
#undef  save_delete
#define save_delete         pPerl->Perl_save_delete
#undef  save_destructor
#define save_destructor     pPerl->Perl_save_destructor
#undef  save_freesv
#define save_freesv         pPerl->Perl_save_freesv
#undef  save_freeop
#define save_freeop         pPerl->Perl_save_freeop
#undef  save_freepv
#define save_freepv         pPerl->Perl_save_freepv
#undef  save_generic_svref
#define save_generic_svref  pPerl->Perl_generic_save_svref
#undef  save_gp
#define save_gp             pPerl->Perl_save_gp
#undef  save_hash
#define save_hash           pPerl->Perl_save_hash
#undef  save_helem
#define save_helem          pPerl->Perl_save_helem
#undef  save_hints
#define save_hints          pPerl->Perl_save_hints
#undef  save_hptr
#define save_hptr           pPerl->Perl_save_hptr
#undef  save_I16
#define save_I16            pPerl->Perl_save_I16
#undef  save_I32
#define save_I32            pPerl->Perl_save_I32
#undef  save_int
#define save_int            pPerl->Perl_save_int
#undef  save_item
#define save_item           pPerl->Perl_save_item
#undef  save_iv
#define save_iv             pPerl->Perl_save_iv
#undef  save_list
#define save_list           pPerl->Perl_save_list
#undef  save_long
#define save_long           pPerl->Perl_save_long
#undef  save_nogv
#define save_nogv           pPerl->Perl_save_nogv
#undef  save_op
#define save_op             pPerl->Perl_save_op
#undef  save_scalar
#define save_scalar         pPerl->Perl_save_scalar
#undef  save_pptr
#define save_pptr           pPerl->Perl_save_pptr
#undef  save_sptr
#define save_sptr           pPerl->Perl_save_sptr
#undef  save_svref
#define save_svref          pPerl->Perl_save_svref
#undef  save_threadsv
#define save_threadsv       pPerl->Perl_save_threadsv
#undef  sawparens
#define sawparens           pPerl->Perl_sawparens
#undef  scalar
#define scalar              pPerl->Perl_scalar
#undef  scalarkids
#define scalarkids          pPerl->Perl_scalarkids
#undef  scalarseq
#define scalarseq           pPerl->Perl_scalarseq
#undef  scalarvoid
#define scalarvoid          pPerl->Perl_scalarvoid
#undef  scan_const
#define scan_const          pPerl->Perl_scan_const
#undef  scan_formline
#define scan_formline       pPerl->Perl_scan_formline
#undef  scan_ident
#define scan_ident          pPerl->Perl_scan_ident
#undef  scan_inputsymbol
#define scan_inputsymbol    pPerl->Perl_scan_inputsymbol
#undef  scan_heredoc
#define scan_heredoc        pPerl->Perl_scan_heredoc
#undef  scan_hex
#define scan_hex            pPerl->Perl_scan_hex
#undef  scan_num
#define scan_num            pPerl->Perl_scan_num
#undef  scan_oct
#define scan_oct            pPerl->Perl_scan_oct
#undef  scan_pat
#define scan_pat            pPerl->Perl_scan_pat
#undef  scan_str
#define scan_str            pPerl->Perl_scan_str
#undef  scan_subst
#define scan_subst          pPerl->Perl_scan_subst
#undef  scan_trans
#define scan_trans          pPerl->Perl_scan_trans
#undef  scope
#define scope               pPerl->Perl_scope
#undef  screaminstr
#define screaminstr         pPerl->Perl_screaminstr
#undef  setdefout
#define setdefout           pPerl->Perl_setdefout
#undef  setenv_getix
#define setenv_getix        pPerl->Perl_setenv_getix
#undef  share_hek
#define share_hek           pPerl->Perl_share_hek
#undef  sharepvn
#define sharepvn            pPerl->Perl_sharepvn
#undef  sighandler
#define sighandler          pPerl->Perl_sighandler
#undef  skipspace
#define skipspace           pPerl->Perl_skipspace
#undef  stack_grow
#define stack_grow          pPerl->Perl_stack_grow
#undef  start_subparse
#define start_subparse      pPerl->Perl_start_subparse
#undef  sub_crush_depth
#define sub_crush_depth     pPerl->Perl_sub_crush_depth
#undef  sublex_done
#define sublex_done         pPerl->Perl_sublex_done
#undef  sublex_start
#define sublex_start        pPerl->Perl_sublex_start
#undef  sv_2bool
#define sv_2bool	    pPerl->Perl_sv_2bool
#undef  sv_2cv
#define sv_2cv		    pPerl->Perl_sv_2cv
#undef  sv_2io
#define sv_2io		    pPerl->Perl_sv_2io
#undef  sv_2iv
#define sv_2iv		    pPerl->Perl_sv_2iv
#undef  sv_2mortal
#define sv_2mortal	    pPerl->Perl_sv_2mortal
#undef  sv_2nv
#define sv_2nv		    pPerl->Perl_sv_2nv
#undef  sv_2pv
#define sv_2pv		    pPerl->Perl_sv_2pv
#undef  sv_2uv
#define sv_2uv		    pPerl->Perl_sv_2uv
#undef  sv_add_arena
#define sv_add_arena	    pPerl->Perl_sv_add_arena
#undef  sv_backoff
#define sv_backoff	    pPerl->Perl_sv_backoff
#undef  sv_bless
#define sv_bless	    pPerl->Perl_sv_bless
#undef  sv_catpv
#define sv_catpv	    pPerl->Perl_sv_catpv
#undef  sv_catpvf
#define sv_catpvf	    pPerl->Perl_sv_catpvf
#undef  sv_catpvn
#define sv_catpvn	    pPerl->Perl_sv_catpvn
#undef  sv_catsv
#define sv_catsv	    pPerl->Perl_sv_catsv
#undef  sv_chop
#define sv_chop		    pPerl->Perl_sv_chop
#undef  sv_clean_all
#define sv_clean_all	    pPerl->Perl_sv_clean_all
#undef  sv_clean_objs
#define sv_clean_objs	    pPerl->Perl_sv_clean_objs
#undef  sv_clear
#define sv_clear	    pPerl->Perl_sv_clear
#undef  sv_cmp
#define sv_cmp		    pPerl->Perl_sv_cmp
#undef  sv_cmp_locale
#define sv_cmp_locale	    pPerl->Perl_sv_cmp_locale
#undef  sv_collxfrm
#define sv_collxfrm	    pPerl->Perl_sv_collxfrm
#undef  sv_compile_2op
#define sv_compile_2op	    pPerl->Perl_sv_compile_2op
#undef  sv_dec
#define sv_dec		    pPerl->Perl_sv_dec
#undef  sv_derived_from
#define sv_derived_from	    pPerl->Perl_sv_derived_from
#undef  sv_dump
#define sv_dump		    pPerl->Perl_sv_dump
#undef  sv_eq
#define sv_eq		    pPerl->Perl_sv_eq
#undef  sv_free
#define sv_free		    pPerl->Perl_sv_free
#undef  sv_free_arenas
#define sv_free_arenas	    pPerl->Perl_sv_free_arenas
#undef  sv_gets
#define sv_gets		    pPerl->Perl_sv_gets
#undef  sv_grow
#define sv_grow		    pPerl->Perl_sv_grow
#undef  sv_inc
#define sv_inc		    pPerl->Perl_sv_inc
#undef  sv_insert
#define sv_insert	    pPerl->Perl_sv_insert
#undef  sv_isa
#define sv_isa		    pPerl->Perl_sv_isa
#undef  sv_isobject
#define sv_isobject	    pPerl->Perl_sv_isobject
#undef  sv_iv
#define sv_iv		    pPerl->Perl_sv_iv
#undef  sv_len
#define sv_len		    pPerl->Perl_sv_len
#undef  sv_magic
#define sv_magic	    pPerl->Perl_sv_magic
#undef  sv_mortalcopy
#define sv_mortalcopy	    pPerl->Perl_sv_mortalcopy
#undef  sv_newmortal
#define sv_newmortal	    pPerl->Perl_sv_newmortal
#undef  sv_newref
#define sv_newref	    pPerl->Perl_sv_newref
#undef  sv_nv
#define sv_nv		    pPerl->Perl_sv_nv
#undef  sv_peek
#define sv_peek		    pPerl->Perl_sv_peek
#undef  sv_pvn
#define sv_pvn		    pPerl->Perl_sv_pvn
#undef  sv_pvn_force
#define sv_pvn_force	    pPerl->Perl_sv_pvn_force
#undef  sv_reftype
#define sv_reftype	    pPerl->Perl_sv_reftype
#undef  sv_replace
#define sv_replace	    pPerl->Perl_sv_replace
#undef  sv_report_used
#define sv_report_used	    pPerl->Perl_sv_report_used
#undef  sv_reset
#define sv_reset	    pPerl->Perl_sv_reset
#undef  sv_setiv
#define sv_setiv	    pPerl->Perl_sv_setiv
#undef  sv_setnv
#define sv_setnv	    pPerl->Perl_sv_setnv
#undef  sv_setpv
#define sv_setpv	    pPerl->Perl_sv_setpv
#undef  sv_setpvf
#define sv_setpvf	    pPerl->Perl_sv_setpvf
#undef  sv_setpviv
#define sv_setpviv	    pPerl->Perl_sv_setpviv
#undef  sv_setpvn
#define sv_setpvn	    pPerl->Perl_sv_setpvn
#undef  sv_setref_iv
#define sv_setref_iv	    pPerl->Perl_sv_setref_iv
#undef  sv_setref_nv
#define sv_setref_nv	    pPerl->Perl_sv_setref_nv
#undef  sv_setref_pv
#define sv_setref_pv	    pPerl->Perl_sv_setref_pv
#undef  sv_setref_pvn
#define sv_setref_pvn	    pPerl->Perl_sv_setref_pvn
#undef  sv_setsv
#define sv_setsv	    pPerl->Perl_sv_setsv
#undef  sv_setuv
#define sv_setuv	    pPerl->Perl_sv_setuv
#undef  sv_taint
#define sv_taint	    pPerl->Perl_sv_taint
#undef  sv_tainted
#define sv_tainted	    pPerl->Perl_sv_tainted
#undef  sv_true
#define sv_true		    pPerl->Perl_sv_true
#undef  sv_unmagic
#define sv_unmagic	    pPerl->Perl_sv_unmagic
#undef  sv_unref
#define sv_unref	    pPerl->Perl_sv_unref
#undef  sv_untaint
#define sv_untaint	    pPerl->Perl_sv_untaint
#undef  sv_upgrade
#define sv_upgrade	    pPerl->Perl_sv_upgrade
#undef  sv_usepvn
#define sv_usepvn	    pPerl->Perl_sv_usepvn
#undef  sv_uv
#define sv_uv		    pPerl->Perl_sv_uv
#undef  sv_vcatpvfn
#define sv_vcatpvfn	    pPerl->Perl_sv_vcatpvfn
#undef  sv_vsetpvfn
#define sv_vsetpvfn	    pPerl->Perl_sv_vsetpvfn
#undef  taint_env
#define taint_env	    pPerl->Perl_taint_env
#undef  taint_not
#define taint_not	    pPerl->Perl_taint_not
#undef  taint_proper
#define taint_proper	    pPerl->Perl_taint_proper
#undef  too_few_arguments
#define too_few_arguments   pPerl->Perl_too_few_arguments
#undef  too_many_arguments
#define too_many_arguments  pPerl->Perl_too_many_arguments
#undef  unlnk
#define unlnk               pPerl->Perl_unlnk
#undef  unlock_condpair
#define unlock_condpair     pPerl->Perl_unlock_condpair
#undef  unshare_hek
#define unshare_hek         pPerl->Perl_unshare_hek
#undef  unsharepvn
#define unsharepvn          pPerl->Perl_unsharepvn
#undef  utilize
#define utilize             pPerl->Perl_utilize
#undef  vivify_defelem
#define vivify_defelem      pPerl->Perl_vivify_defelem
#undef  vivify_ref
#define vivify_ref          pPerl->Perl_vivify_ref
#undef  wait4pid
#define wait4pid            pPerl->Perl_wait4pid
#undef  warn
#define warn    	    pPerl->Perl_warn
#undef  watch
#define watch    	    pPerl->Perl_watch
#undef  whichsig
#define whichsig            pPerl->Perl_whichsig
#undef  yyerror
#define yyerror             pPerl->Perl_yyerror
#undef  yylex
#define yylex               pPerl->Perl_yylex
#undef  yyparse
#define yyparse             pPerl->Perl_yyparse
#undef  yywarn
#define yywarn              pPerl->Perl_yywarn


#undef  PL_piMem
#define PL_piMem               (pPerl->PL_piMem)
#undef  PL_piENV
#define PL_piENV               (pPerl->PL_piENV)
#undef  PL_piStdIO
#define PL_piStdIO             (pPerl->PL_piStdIO)
#undef  PL_piLIO
#define PL_piLIO               (pPerl->PL_piLIO)
#undef  PL_piDir
#define PL_piDir               (pPerl->PL_piDir)
#undef  PL_piSock
#define PL_piSock              (pPerl->PL_piSock)
#undef  PL_piProc
#define PL_piProc              (pPerl->PL_piProc)

#ifndef NO_XSLOCKS
#undef closedir
#undef opendir
#undef stdin
#undef stdout
#undef stderr
#undef feof
#undef ferror
#undef fgetpos
#undef ioctl
#undef getlogin
#undef setjmp
#undef getc
#undef ungetc
#undef fileno

#define mkdir PerlDir_mkdir
#define chdir PerlDir_chdir
#define rmdir PerlDir_rmdir
#define closedir PerlDir_close
#define opendir PerlDir_open
#define readdir PerlDir_read
#define rewinddir PerlDir_rewind
#define seekdir PerlDir_seek
#define telldir PerlDir_tell
#define putenv PerlEnv_putenv
#define getenv PerlEnv_getenv
#define stdin PerlIO_stdin()
#define stdout PerlIO_stdout()
#define stderr PerlIO_stderr()
#define fopen PerlIO_open
#define fclose PerlIO_close
#define feof PerlIO_eof
#define ferror PerlIO_error
#define fclearerr PerlIO_clearerr
#define getc PerlIO_getc
#define fputc(c, f) PerlIO_putc(f,c)
#define fputs(s, f) PerlIO_puts(f,s)
#define fflush PerlIO_flush
#define ungetc(c, f) PerlIO_ungetc((f),(c))
#define fileno PerlIO_fileno
#define fdopen PerlIO_fdopen
#define freopen PerlIO_reopen
#define fread(b,s,c,f) PerlIO_read((f),(b),(s*c))
#define fwrite(b,s,c,f) PerlIO_write((f),(b),(s*c))
#define setbuf PerlIO_setbuf
#define setvbuf PerlIO_setvbuf
#define setlinebuf PerlIO_setlinebuf
#define stdoutf PerlIO_stdoutf
#define vfprintf PerlIO_vprintf
#define ftell PerlIO_tell
#define fseek PerlIO_seek
#define fgetpos PerlIO_getpos
#define fsetpos PerlIO_setpos
#define frewind PerlIO_rewind
#define tmpfile PerlIO_tmpfile
#define access PerlLIO_access
#define chmod PerlLIO_chmod
#define chsize PerlLIO_chsize
#define close PerlLIO_close
#define dup PerlLIO_dup
#define dup2 PerlLIO_dup2
#define flock PerlLIO_flock
#define fstat PerlLIO_fstat
#define ioctl PerlLIO_ioctl
#define isatty PerlLIO_isatty
#define lseek PerlLIO_lseek
#define lstat PerlLIO_lstat
#define mktemp PerlLIO_mktemp
#define open PerlLIO_open
#define read PerlLIO_read
#define rename PerlLIO_rename
#define setmode PerlLIO_setmode
#define stat PerlLIO_stat
#define tmpnam PerlLIO_tmpnam
#define umask PerlLIO_umask
#define unlink PerlLIO_unlink
#define utime PerlLIO_utime
#define write PerlLIO_write
#define malloc PerlMem_malloc
#define realloc PerlMem_realloc
#define free PerlMem_free
#define abort PerlProc_abort
#define exit PerlProc_exit
#define _exit PerlProc__exit
#define execl PerlProc_execl
#define execv PerlProc_execv
#define execvp PerlProc_execvp
#define getuid PerlProc_getuid
#define geteuid PerlProc_geteuid
#define getgid PerlProc_getgid
#define getegid PerlProc_getegid
#define getlogin PerlProc_getlogin
#define kill PerlProc_kill
#define killpg PerlProc_killpg
#define pause PerlProc_pause
#define popen PerlProc_popen
#define pclose PerlProc_pclose
#define pipe PerlProc_pipe
#define setuid PerlProc_setuid
#define setgid PerlProc_setgid
#define sleep PerlProc_sleep
#define times PerlProc_times
#define wait PerlProc_wait
#define setjmp PerlProc_setjmp
#define longjmp PerlProc_longjmp
#define signal PerlProc_signal
#define htonl PerlSock_htonl
#define htons PerlSock_htons
#define ntohl PerlSock_ntohl
#define ntohs PerlSock_ntohs
#define accept PerlSock_accept
#define bind PerlSock_bind
#define connect PerlSock_connect
#define endhostent PerlSock_endhostent
#define endnetent PerlSock_endnetent
#define endprotoent PerlSock_endprotoent
#define endservent PerlSock_endservent
#define gethostbyaddr PerlSock_gethostbyaddr
#define gethostbyname PerlSock_gethostbyname
#define gethostent PerlSock_gethostent
#define gethostname PerlSock_gethostname
#define getnetbyaddr PerlSock_getnetbyaddr
#define getnetbyname PerlSock_getnetbyname
#define getnetent PerlSock_getnetent
#define getpeername PerlSock_getpeername
#define getprotobyname PerlSock_getprotobyname
#define getprotobynumber PerlSock_getprotobynumber
#define getprotoent PerlSock_getprotoent
#define getservbyname PerlSock_getservbyname
#define getservbyport PerlSock_getservbyport
#define getservent PerlSock_getservent
#define getsockname PerlSock_getsockname
#define getsockopt PerlSock_getsockopt
#define inet_addr PerlSock_inet_addr
#define inet_ntoa PerlSock_inet_ntoa
#define listen PerlSock_listen
#define recvfrom PerlSock_recvfrom
#define select PerlSock_select
#define send PerlSock_send
#define sendto PerlSock_sendto
#define sethostent PerlSock_sethostent
#define setnetent PerlSock_setnetent
#define setprotoent PerlSock_setprotoent
#define setservent PerlSock_setservent
#define setsockopt PerlSock_setsockopt
#define shutdown PerlSock_shutdown
#define socket PerlSock_socket
#define socketpair PerlSock_socketpair
#endif  /* NO_XSLOCKS */

#undef  PERL_OBJECT_THIS
#define PERL_OBJECT_THIS pPerl
#undef  PERL_OBJECT_THIS_
#define PERL_OBJECT_THIS_ pPerl,

#undef  SAVEDESTRUCTOR
#define SAVEDESTRUCTOR(f,p) \
	pPerl->Perl_save_destructor((FUNC_NAME_TO_PTR(f)),(p))

#ifdef WIN32

#ifndef WIN32IO_IS_STDIO
#undef	errno
#define errno                 ErrorNo()
#endif

#undef  ErrorNo
#define ErrorNo				pPerl->ErrorNo
#undef  NtCrypt
#define NtCrypt               pPerl->NtCrypt
#undef  NtGetLib
#define NtGetLib              pPerl->NtGetLib
#undef  NtGetArchLib
#define NtGetArchLib          pPerl->NtGetArchLib
#undef  NtGetSiteLib
#define NtGetSiteLib          pPerl->NtGetSiteLib
#undef  NtGetBin
#define NtGetBin              pPerl->NtGetBin
#undef  NtGetDebugScriptStr
#define NtGetDebugScriptStr   pPerl->NtGetDebugScriptStr
#endif /* WIN32 */

#endif	/* __objXSUB_h__ */ 

