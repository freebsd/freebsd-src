/*
 * term.h 
 *
 * This file was generated automatically.
 *
 */

#ifndef _TERM_H_
#define _TERM_H_

#define auto_left_margin                 (_CUR_TERM.bools[0])
#define auto_right_margin                (_CUR_TERM.bools[1])
#define ceol_standout_glitch             (_CUR_TERM.bools[2])
#define dest_tabs_magic_smso             (_CUR_TERM.bools[3])
#define eat_newline_glitch               (_CUR_TERM.bools[4])
#define erase_overstrike                 (_CUR_TERM.bools[5])
#define generic_type                     (_CUR_TERM.bools[6])
#define hard_copy                        (_CUR_TERM.bools[7])
#define hard_cursor                      (_CUR_TERM.bools[8])
#define has_meta_key                     (_CUR_TERM.bools[9])
#define has_status_line                  (_CUR_TERM.bools[10])
#define insert_null_glitch               (_CUR_TERM.bools[11])
#define memory_above                     (_CUR_TERM.bools[12])
#define memory_below                     (_CUR_TERM.bools[13])
#define move_insert_mode                 (_CUR_TERM.bools[14])
#define move_standout_mode               (_CUR_TERM.bools[15])
#define needs_xon_xoff                   (_CUR_TERM.bools[16])
#define no_esc_ctlc                      (_CUR_TERM.bools[17])
#define no_pad_char                      (_CUR_TERM.bools[18])
#define non_rev_rmcup                    (_CUR_TERM.bools[19])
#define over_strike                      (_CUR_TERM.bools[20])
#define prtr_silent                      (_CUR_TERM.bools[21])
#define status_line_esc_ok               (_CUR_TERM.bools[22])
#define tilde_glitch                     (_CUR_TERM.bools[23])
#define transparent_underline            (_CUR_TERM.bools[24])
#define xon_xoff                         (_CUR_TERM.bools[25])
#define back_color_erase                 (_CUR_TERM.bools[26])
#define can_change                       (_CUR_TERM.bools[27])
#define col_addr_glitch                  (_CUR_TERM.bools[28])
#define cpi_changes_res                  (_CUR_TERM.bools[29])
#define cr_cancels_micro_mode            (_CUR_TERM.bools[30])
#define has_print_wheel                  (_CUR_TERM.bools[31])
#define hue_lightness_saturation         (_CUR_TERM.bools[32])
#define lpi_changes_res                  (_CUR_TERM.bools[33])
#define non_dest_scroll_region           (_CUR_TERM.bools[34])
#define row_addr_glitch                  (_CUR_TERM.bools[35])
#define semi_auto_right_margin           (_CUR_TERM.bools[36])
#define backspaces_with_bs               (_CUR_TERM.bools[37])
#define even_parity                      (_CUR_TERM.bools[38])
#define half_duplex                      (_CUR_TERM.bools[39])
#define lower_case_only                  (_CUR_TERM.bools[40])
#define no_correctly_working_cr          (_CUR_TERM.bools[41])
#define linefeed_is_newline              (_CUR_TERM.bools[42])
#define crt_without_scrolling            (_CUR_TERM.bools[43])
#define odd_parity                       (_CUR_TERM.bools[44])
#define has_hardware_tabs                (_CUR_TERM.bools[45])
#define uppercase_only                   (_CUR_TERM.bools[46])
#define return_does_clr_eol              (_CUR_TERM.bools[47])
#define tek_4025_insert_line             (_CUR_TERM.bools[48])
#define initialization_messy             (_CUR_TERM.bools[49])
#define index_at_bottom_does_cr          (_CUR_TERM.bools[50])
#define rind_only_at_top                 (_CUR_TERM.bools[51])
#define GNU_has_meta_key                 (_CUR_TERM.bools[52])
#define columns                          (_CUR_TERM.nums[0])
#define init_tabs                        (_CUR_TERM.nums[1])
#define label_height                     (_CUR_TERM.nums[2])
#define label_width                      (_CUR_TERM.nums[3])
#define lines                            (_CUR_TERM.nums[4])
#define lines_of_memory                  (_CUR_TERM.nums[5])
#define magic_cookie_glitch              (_CUR_TERM.nums[6])
#define num_labels                       (_CUR_TERM.nums[7])
#define padding_baud_rate                (_CUR_TERM.nums[8])
#define virtual_terminal                 (_CUR_TERM.nums[9])
#define width_status_line                (_CUR_TERM.nums[10])
#define bit_image_entwining              (_CUR_TERM.nums[11])
#define bit_image_type                   (_CUR_TERM.nums[12])
#define buffer_capacity                  (_CUR_TERM.nums[13])
#define buttons                          (_CUR_TERM.nums[14])
#define dot_horz_spacing                 (_CUR_TERM.nums[15])
#define dot_vert_spacing                 (_CUR_TERM.nums[16])
#define max_attributes                   (_CUR_TERM.nums[17])
#define max_colors                       (_CUR_TERM.nums[18])
#define max_micro_address                (_CUR_TERM.nums[19])
#define max_micro_jump                   (_CUR_TERM.nums[20])
#define max_pairs                        (_CUR_TERM.nums[21])
#define maximum_windows                  (_CUR_TERM.nums[22])
#define micro_char_size                  (_CUR_TERM.nums[23])
#define micro_line_size                  (_CUR_TERM.nums[24])
#define no_color_video                   (_CUR_TERM.nums[25])
#define number_of_pins                   (_CUR_TERM.nums[26])
#define output_res_char                  (_CUR_TERM.nums[27])
#define output_res_horz_inch             (_CUR_TERM.nums[28])
#define output_res_line                  (_CUR_TERM.nums[29])
#define output_res_vert_inch             (_CUR_TERM.nums[30])
#define print_rate                       (_CUR_TERM.nums[31])
#define wide_char_size                   (_CUR_TERM.nums[32])
#define backspace_delay                  (_CUR_TERM.nums[33])
#define carriage_return_delay            (_CUR_TERM.nums[34])
#define form_feed_delay                  (_CUR_TERM.nums[35])
#define new_line_delay                   (_CUR_TERM.nums[36])
#define horizontal_tab_delay             (_CUR_TERM.nums[37])
#define vertical_tab_delay               (_CUR_TERM.nums[38])
#define number_of_function_keys          (_CUR_TERM.nums[39])
#define magic_cookie_glitch_ul           (_CUR_TERM.nums[40])
#define GNU_tab_width                    (_CUR_TERM.nums[41])
#define acs_chars                        (_CUR_TERM.strs[0])
#define back_tab                         (_CUR_TERM.strs[1])
#define bell                             (_CUR_TERM.strs[2])
#define carriage_return                  (_CUR_TERM.strs[3])
#define change_scroll_region             (_CUR_TERM.strs[4])
#define char_padding                     (_CUR_TERM.strs[5])
#define clear_all_tabs                   (_CUR_TERM.strs[6])
#define clear_margins                    (_CUR_TERM.strs[7])
#define clear_screen                     (_CUR_TERM.strs[8])
#define clr_bol                          (_CUR_TERM.strs[9])
#define clr_eol                          (_CUR_TERM.strs[10])
#define clr_eos                          (_CUR_TERM.strs[11])
#define column_address                   (_CUR_TERM.strs[12])
#define command_character                (_CUR_TERM.strs[13])
#define cursor_address                   (_CUR_TERM.strs[14])
#define cursor_down                      (_CUR_TERM.strs[15])
#define cursor_home                      (_CUR_TERM.strs[16])
#define cursor_invisible                 (_CUR_TERM.strs[17])
#define cursor_left                      (_CUR_TERM.strs[18])
#define cursor_mem_address               (_CUR_TERM.strs[19])
#define cursor_normal                    (_CUR_TERM.strs[20])
#define cursor_right                     (_CUR_TERM.strs[21])
#define cursor_to_ll                     (_CUR_TERM.strs[22])
#define cursor_up                        (_CUR_TERM.strs[23])
#define cursor_visible                   (_CUR_TERM.strs[24])
#define delete_character                 (_CUR_TERM.strs[25])
#define delete_line                      (_CUR_TERM.strs[26])
#define dis_status_line                  (_CUR_TERM.strs[27])
#define down_half_line                   (_CUR_TERM.strs[28])
#define ena_acs                          (_CUR_TERM.strs[29])
#define enter_alt_charset_mode           (_CUR_TERM.strs[30])
#define enter_am_mode                    (_CUR_TERM.strs[31])
#define enter_blink_mode                 (_CUR_TERM.strs[32])
#define enter_bold_mode                  (_CUR_TERM.strs[33])
#define enter_ca_mode                    (_CUR_TERM.strs[34])
#define enter_delete_mode                (_CUR_TERM.strs[35])
#define enter_dim_mode                   (_CUR_TERM.strs[36])
#define enter_insert_mode                (_CUR_TERM.strs[37])
#define enter_protected_mode             (_CUR_TERM.strs[38])
#define enter_reverse_mode               (_CUR_TERM.strs[39])
#define enter_secure_mode                (_CUR_TERM.strs[40])
#define enter_standout_mode              (_CUR_TERM.strs[41])
#define enter_underline_mode             (_CUR_TERM.strs[42])
#define enter_xon_mode                   (_CUR_TERM.strs[43])
#define erase_chars                      (_CUR_TERM.strs[44])
#define exit_alt_charset_mode            (_CUR_TERM.strs[45])
#define exit_am_mode                     (_CUR_TERM.strs[46])
#define exit_attribute_mode              (_CUR_TERM.strs[47])
#define exit_ca_mode                     (_CUR_TERM.strs[48])
#define exit_delete_mode                 (_CUR_TERM.strs[49])
#define exit_insert_mode                 (_CUR_TERM.strs[50])
#define exit_standout_mode               (_CUR_TERM.strs[51])
#define exit_underline_mode              (_CUR_TERM.strs[52])
#define exit_xon_mode                    (_CUR_TERM.strs[53])
#define flash_screen                     (_CUR_TERM.strs[54])
#define form_feed                        (_CUR_TERM.strs[55])
#define from_status_line                 (_CUR_TERM.strs[56])
#define init_1string                     (_CUR_TERM.strs[57])
#define init_2string                     (_CUR_TERM.strs[58])
#define init_3string                     (_CUR_TERM.strs[59])
#define init_file                        (_CUR_TERM.strs[60])
#define init_prog                        (_CUR_TERM.strs[61])
#define insert_character                 (_CUR_TERM.strs[62])
#define insert_line                      (_CUR_TERM.strs[63])
#define insert_padding                   (_CUR_TERM.strs[64])
#define key_a1                           (_CUR_TERM.strs[65])
#define key_a3                           (_CUR_TERM.strs[66])
#define key_b2                           (_CUR_TERM.strs[67])
#define key_backspace                    (_CUR_TERM.strs[68])
#define key_beg                          (_CUR_TERM.strs[69])
#define key_btab                         (_CUR_TERM.strs[70])
#define key_c1                           (_CUR_TERM.strs[71])
#define key_c3                           (_CUR_TERM.strs[72])
#define key_cancel                       (_CUR_TERM.strs[73])
#define key_catab                        (_CUR_TERM.strs[74])
#define key_clear                        (_CUR_TERM.strs[75])
#define key_close                        (_CUR_TERM.strs[76])
#define key_command                      (_CUR_TERM.strs[77])
#define key_copy                         (_CUR_TERM.strs[78])
#define key_create                       (_CUR_TERM.strs[79])
#define key_ctab                         (_CUR_TERM.strs[80])
#define key_dc                           (_CUR_TERM.strs[81])
#define key_dl                           (_CUR_TERM.strs[82])
#define key_down                         (_CUR_TERM.strs[83])
#define key_eic                          (_CUR_TERM.strs[84])
#define key_end                          (_CUR_TERM.strs[85])
#define key_enter                        (_CUR_TERM.strs[86])
#define key_eol                          (_CUR_TERM.strs[87])
#define key_eos                          (_CUR_TERM.strs[88])
#define key_exit                         (_CUR_TERM.strs[89])
#define key_f0                           (_CUR_TERM.strs[90])
#define key_f1                           (_CUR_TERM.strs[91])
#define key_f10                          (_CUR_TERM.strs[92])
#define key_f11                          (_CUR_TERM.strs[93])
#define key_f12                          (_CUR_TERM.strs[94])
#define key_f13                          (_CUR_TERM.strs[95])
#define key_f14                          (_CUR_TERM.strs[96])
#define key_f15                          (_CUR_TERM.strs[97])
#define key_f16                          (_CUR_TERM.strs[98])
#define key_f17                          (_CUR_TERM.strs[99])
#define key_f18                          (_CUR_TERM.strs[100])
#define key_f19                          (_CUR_TERM.strs[101])
#define key_f2                           (_CUR_TERM.strs[102])
#define key_f20                          (_CUR_TERM.strs[103])
#define key_f21                          (_CUR_TERM.strs[104])
#define key_f22                          (_CUR_TERM.strs[105])
#define key_f23                          (_CUR_TERM.strs[106])
#define key_f24                          (_CUR_TERM.strs[107])
#define key_f25                          (_CUR_TERM.strs[108])
#define key_f26                          (_CUR_TERM.strs[109])
#define key_f27                          (_CUR_TERM.strs[110])
#define key_f28                          (_CUR_TERM.strs[111])
#define key_f29                          (_CUR_TERM.strs[112])
#define key_f3                           (_CUR_TERM.strs[113])
#define key_f30                          (_CUR_TERM.strs[114])
#define key_f31                          (_CUR_TERM.strs[115])
#define key_f32                          (_CUR_TERM.strs[116])
#define key_f33                          (_CUR_TERM.strs[117])
#define key_f34                          (_CUR_TERM.strs[118])
#define key_f35                          (_CUR_TERM.strs[119])
#define key_f36                          (_CUR_TERM.strs[120])
#define key_f37                          (_CUR_TERM.strs[121])
#define key_f38                          (_CUR_TERM.strs[122])
#define key_f39                          (_CUR_TERM.strs[123])
#define key_f4                           (_CUR_TERM.strs[124])
#define key_f40                          (_CUR_TERM.strs[125])
#define key_f41                          (_CUR_TERM.strs[126])
#define key_f42                          (_CUR_TERM.strs[127])
#define key_f43                          (_CUR_TERM.strs[128])
#define key_f44                          (_CUR_TERM.strs[129])
#define key_f45                          (_CUR_TERM.strs[130])
#define key_f46                          (_CUR_TERM.strs[131])
#define key_f47                          (_CUR_TERM.strs[132])
#define key_f48                          (_CUR_TERM.strs[133])
#define key_f49                          (_CUR_TERM.strs[134])
#define key_f5                           (_CUR_TERM.strs[135])
#define key_f50                          (_CUR_TERM.strs[136])
#define key_f51                          (_CUR_TERM.strs[137])
#define key_f52                          (_CUR_TERM.strs[138])
#define key_f53                          (_CUR_TERM.strs[139])
#define key_f54                          (_CUR_TERM.strs[140])
#define key_f55                          (_CUR_TERM.strs[141])
#define key_f56                          (_CUR_TERM.strs[142])
#define key_f57                          (_CUR_TERM.strs[143])
#define key_f58                          (_CUR_TERM.strs[144])
#define key_f59                          (_CUR_TERM.strs[145])
#define key_f6                           (_CUR_TERM.strs[146])
#define key_f60                          (_CUR_TERM.strs[147])
#define key_f61                          (_CUR_TERM.strs[148])
#define key_f62                          (_CUR_TERM.strs[149])
#define key_f63                          (_CUR_TERM.strs[150])
#define key_f7                           (_CUR_TERM.strs[151])
#define key_f8                           (_CUR_TERM.strs[152])
#define key_f9                           (_CUR_TERM.strs[153])
#define key_find                         (_CUR_TERM.strs[154])
#define key_help                         (_CUR_TERM.strs[155])
#define key_home                         (_CUR_TERM.strs[156])
#define key_ic                           (_CUR_TERM.strs[157])
#define key_il                           (_CUR_TERM.strs[158])
#define key_left                         (_CUR_TERM.strs[159])
#define key_ll                           (_CUR_TERM.strs[160])
#define key_mark                         (_CUR_TERM.strs[161])
#define key_message                      (_CUR_TERM.strs[162])
#define key_move                         (_CUR_TERM.strs[163])
#define key_next                         (_CUR_TERM.strs[164])
#define key_npage                        (_CUR_TERM.strs[165])
#define key_open                         (_CUR_TERM.strs[166])
#define key_options                      (_CUR_TERM.strs[167])
#define key_ppage                        (_CUR_TERM.strs[168])
#define key_previous                     (_CUR_TERM.strs[169])
#define key_print                        (_CUR_TERM.strs[170])
#define key_redo                         (_CUR_TERM.strs[171])
#define key_reference                    (_CUR_TERM.strs[172])
#define key_refresh                      (_CUR_TERM.strs[173])
#define key_replace                      (_CUR_TERM.strs[174])
#define key_restart                      (_CUR_TERM.strs[175])
#define key_resume                       (_CUR_TERM.strs[176])
#define key_right                        (_CUR_TERM.strs[177])
#define key_save                         (_CUR_TERM.strs[178])
#define key_sbeg                         (_CUR_TERM.strs[179])
#define key_scancel                      (_CUR_TERM.strs[180])
#define key_scommand                     (_CUR_TERM.strs[181])
#define key_scopy                        (_CUR_TERM.strs[182])
#define key_screate                      (_CUR_TERM.strs[183])
#define key_sdc                          (_CUR_TERM.strs[184])
#define key_sdl                          (_CUR_TERM.strs[185])
#define key_select                       (_CUR_TERM.strs[186])
#define key_send                         (_CUR_TERM.strs[187])
#define key_seol                         (_CUR_TERM.strs[188])
#define key_sexit                        (_CUR_TERM.strs[189])
#define key_sf                           (_CUR_TERM.strs[190])
#define key_sfind                        (_CUR_TERM.strs[191])
#define key_shelp                        (_CUR_TERM.strs[192])
#define key_shome                        (_CUR_TERM.strs[193])
#define key_sic                          (_CUR_TERM.strs[194])
#define key_sleft                        (_CUR_TERM.strs[195])
#define key_smessage                     (_CUR_TERM.strs[196])
#define key_smove                        (_CUR_TERM.strs[197])
#define key_snext                        (_CUR_TERM.strs[198])
#define key_soptions                     (_CUR_TERM.strs[199])
#define key_sprevious                    (_CUR_TERM.strs[200])
#define key_sprint                       (_CUR_TERM.strs[201])
#define key_sr                           (_CUR_TERM.strs[202])
#define key_sredo                        (_CUR_TERM.strs[203])
#define key_sreplace                     (_CUR_TERM.strs[204])
#define key_sright                       (_CUR_TERM.strs[205])
#define key_srsume                       (_CUR_TERM.strs[206])
#define key_ssave                        (_CUR_TERM.strs[207])
#define key_ssuspend                     (_CUR_TERM.strs[208])
#define key_stab                         (_CUR_TERM.strs[209])
#define key_sundo                        (_CUR_TERM.strs[210])
#define key_suspend                      (_CUR_TERM.strs[211])
#define key_undo                         (_CUR_TERM.strs[212])
#define key_up                           (_CUR_TERM.strs[213])
#define keypad_local                     (_CUR_TERM.strs[214])
#define keypad_xmit                      (_CUR_TERM.strs[215])
#define lab_f0                           (_CUR_TERM.strs[216])
#define lab_f1                           (_CUR_TERM.strs[217])
#define lab_f10                          (_CUR_TERM.strs[218])
#define lab_f2                           (_CUR_TERM.strs[219])
#define lab_f3                           (_CUR_TERM.strs[220])
#define lab_f4                           (_CUR_TERM.strs[221])
#define lab_f5                           (_CUR_TERM.strs[222])
#define lab_f6                           (_CUR_TERM.strs[223])
#define lab_f7                           (_CUR_TERM.strs[224])
#define lab_f8                           (_CUR_TERM.strs[225])
#define lab_f9                           (_CUR_TERM.strs[226])
#define label_off                        (_CUR_TERM.strs[227])
#define label_on                         (_CUR_TERM.strs[228])
#define meta_off                         (_CUR_TERM.strs[229])
#define meta_on                          (_CUR_TERM.strs[230])
#define newline                          (_CUR_TERM.strs[231])
#define pad_char                         (_CUR_TERM.strs[232])
#define parm_dch                         (_CUR_TERM.strs[233])
#define parm_delete_line                 (_CUR_TERM.strs[234])
#define parm_down_cursor                 (_CUR_TERM.strs[235])
#define parm_ich                         (_CUR_TERM.strs[236])
#define parm_index                       (_CUR_TERM.strs[237])
#define parm_insert_line                 (_CUR_TERM.strs[238])
#define parm_left_cursor                 (_CUR_TERM.strs[239])
#define parm_right_cursor                (_CUR_TERM.strs[240])
#define parm_rindex                      (_CUR_TERM.strs[241])
#define parm_up_cursor                   (_CUR_TERM.strs[242])
#define pkey_key                         (_CUR_TERM.strs[243])
#define pkey_local                       (_CUR_TERM.strs[244])
#define pkey_xmit                        (_CUR_TERM.strs[245])
#define plab_norm                        (_CUR_TERM.strs[246])
#define print_screen                     (_CUR_TERM.strs[247])
#define prtr_non                         (_CUR_TERM.strs[248])
#define prtr_off                         (_CUR_TERM.strs[249])
#define prtr_on                          (_CUR_TERM.strs[250])
#define repeat_char                      (_CUR_TERM.strs[251])
#define req_for_input                    (_CUR_TERM.strs[252])
#define reset_1string                    (_CUR_TERM.strs[253])
#define reset_2string                    (_CUR_TERM.strs[254])
#define reset_3string                    (_CUR_TERM.strs[255])
#define reset_file                       (_CUR_TERM.strs[256])
#define restore_cursor                   (_CUR_TERM.strs[257])
#define row_address                      (_CUR_TERM.strs[258])
#define save_cursor                      (_CUR_TERM.strs[259])
#define scroll_forward                   (_CUR_TERM.strs[260])
#define scroll_reverse                   (_CUR_TERM.strs[261])
#define set_attributes                   (_CUR_TERM.strs[262])
#define set_left_margin                  (_CUR_TERM.strs[263])
#define set_right_margin                 (_CUR_TERM.strs[264])
#define set_tab                          (_CUR_TERM.strs[265])
#define set_window                       (_CUR_TERM.strs[266])
#define tab                              (_CUR_TERM.strs[267])
#define to_status_line                   (_CUR_TERM.strs[268])
#define underline_char                   (_CUR_TERM.strs[269])
#define up_half_line                     (_CUR_TERM.strs[270])
#define xoff_character                   (_CUR_TERM.strs[271])
#define xon_character                    (_CUR_TERM.strs[272])
#define alt_scancode_esc                 (_CUR_TERM.strs[273])
#define bit_image_carriage_return        (_CUR_TERM.strs[274])
#define bit_image_newline                (_CUR_TERM.strs[275])
#define bit_image_repeat                 (_CUR_TERM.strs[276])
#define change_char_pitch                (_CUR_TERM.strs[277])
#define change_line_pitch                (_CUR_TERM.strs[278])
#define change_res_horz                  (_CUR_TERM.strs[279])
#define change_res_vert                  (_CUR_TERM.strs[280])
#define char_set_names                   (_CUR_TERM.strs[281])
#define code_set_init                    (_CUR_TERM.strs[282])
#define color_names                      (_CUR_TERM.strs[283])
#define create_window                    (_CUR_TERM.strs[284])
#define define_bit_image_region          (_CUR_TERM.strs[285])
#define define_char                      (_CUR_TERM.strs[286])
#define device_type                      (_CUR_TERM.strs[287])
#define dial_phone                       (_CUR_TERM.strs[288])
#define display_clock                    (_CUR_TERM.strs[289])
#define display_pc_char                  (_CUR_TERM.strs[290])
#define end_bit_image_region             (_CUR_TERM.strs[291])
#define enter_doublewide_mode            (_CUR_TERM.strs[292])
#define enter_draft_quality              (_CUR_TERM.strs[293])
#define enter_italics_mode               (_CUR_TERM.strs[294])
#define enter_leftward_mode              (_CUR_TERM.strs[295])
#define enter_micro_mode                 (_CUR_TERM.strs[296])
#define enter_near_letter_quality        (_CUR_TERM.strs[297])
#define enter_normal_quality             (_CUR_TERM.strs[298])
#define enter_pc_charset_mode            (_CUR_TERM.strs[299])
#define enter_scancode_mode              (_CUR_TERM.strs[300])
#define enter_shadow_mode                (_CUR_TERM.strs[301])
#define enter_subscript_mode             (_CUR_TERM.strs[302])
#define enter_superscript_mode           (_CUR_TERM.strs[303])
#define enter_upward_mode                (_CUR_TERM.strs[304])
#define exit_doublewide_mode             (_CUR_TERM.strs[305])
#define exit_italics_mode                (_CUR_TERM.strs[306])
#define exit_leftward_mode               (_CUR_TERM.strs[307])
#define exit_micro_mode                  (_CUR_TERM.strs[308])
#define exit_pc_charset_mode             (_CUR_TERM.strs[309])
#define exit_scancode_mode               (_CUR_TERM.strs[310])
#define exit_shadow_mode                 (_CUR_TERM.strs[311])
#define exit_subscript_mode              (_CUR_TERM.strs[312])
#define exit_superscript_mode            (_CUR_TERM.strs[313])
#define exit_upward_mode                 (_CUR_TERM.strs[314])
#define fixed_pause                      (_CUR_TERM.strs[315])
#define flash_hook                       (_CUR_TERM.strs[316])
#define get_mouse                        (_CUR_TERM.strs[317])
#define goto_window                      (_CUR_TERM.strs[318])
#define hangup                           (_CUR_TERM.strs[319])
#define initialize_color                 (_CUR_TERM.strs[320])
#define initialize_pair                  (_CUR_TERM.strs[321])
#define key_mouse                        (_CUR_TERM.strs[322])
#define label_format                     (_CUR_TERM.strs[323])
#define micro_column_address             (_CUR_TERM.strs[324])
#define micro_down                       (_CUR_TERM.strs[325])
#define micro_left                       (_CUR_TERM.strs[326])
#define micro_right                      (_CUR_TERM.strs[327])
#define micro_row_address                (_CUR_TERM.strs[328])
#define micro_up                         (_CUR_TERM.strs[329])
#define mouse_info                       (_CUR_TERM.strs[330])
#define order_of_pins                    (_CUR_TERM.strs[331])
#define orig_colors                      (_CUR_TERM.strs[332])
#define orig_pair                        (_CUR_TERM.strs[333])
#define parm_down_micro                  (_CUR_TERM.strs[334])
#define parm_left_micro                  (_CUR_TERM.strs[335])
#define parm_right_micro                 (_CUR_TERM.strs[336])
#define parm_up_micro                    (_CUR_TERM.strs[337])
#define pc_term_options                  (_CUR_TERM.strs[338])
#define pkey_plab                        (_CUR_TERM.strs[339])
#define pulse                            (_CUR_TERM.strs[340])
#define quick_dial                       (_CUR_TERM.strs[341])
#define remove_clock                     (_CUR_TERM.strs[342])
#define req_mouse_pos                    (_CUR_TERM.strs[343])
#define scancode_escape                  (_CUR_TERM.strs[344])
#define select_char_set                  (_CUR_TERM.strs[345])
#define set0_des_seq                     (_CUR_TERM.strs[346])
#define set1_des_seq                     (_CUR_TERM.strs[347])
#define set2_des_seq                     (_CUR_TERM.strs[348])
#define set3_des_seq                     (_CUR_TERM.strs[349])
#define set_a_background                 (_CUR_TERM.strs[350])
#define set_a_foreground                 (_CUR_TERM.strs[351])
#define set_background                   (_CUR_TERM.strs[352])
#define set_bottom_margin                (_CUR_TERM.strs[353])
#define set_bottom_margin_parm           (_CUR_TERM.strs[354])
#define set_clock                        (_CUR_TERM.strs[355])
#define set_color_band                   (_CUR_TERM.strs[356])
#define set_color_pair                   (_CUR_TERM.strs[357])
#define set_foreground                   (_CUR_TERM.strs[358])
#define set_left_margin_parm             (_CUR_TERM.strs[359])
#define set_lr_margin                    (_CUR_TERM.strs[360])
#define set_page_length                  (_CUR_TERM.strs[361])
#define set_right_margin_parm            (_CUR_TERM.strs[362])
#define set_tb_margin                    (_CUR_TERM.strs[363])
#define set_top_margin                   (_CUR_TERM.strs[364])
#define set_top_margin_parm              (_CUR_TERM.strs[365])
#define start_bit_image                  (_CUR_TERM.strs[366])
#define start_char_set_def               (_CUR_TERM.strs[367])
#define stop_bit_image                   (_CUR_TERM.strs[368])
#define stop_char_set_def                (_CUR_TERM.strs[369])
#define subscript_characters             (_CUR_TERM.strs[370])
#define superscript_characters           (_CUR_TERM.strs[371])
#define these_cause_cr                   (_CUR_TERM.strs[372])
#define tone                             (_CUR_TERM.strs[373])
#define user0                            (_CUR_TERM.strs[374])
#define user1                            (_CUR_TERM.strs[375])
#define user2                            (_CUR_TERM.strs[376])
#define user3                            (_CUR_TERM.strs[377])
#define user4                            (_CUR_TERM.strs[378])
#define user5                            (_CUR_TERM.strs[379])
#define user6                            (_CUR_TERM.strs[380])
#define user7                            (_CUR_TERM.strs[381])
#define user8                            (_CUR_TERM.strs[382])
#define user9                            (_CUR_TERM.strs[383])
#define wait_tone                        (_CUR_TERM.strs[384])
#define zero_motion                      (_CUR_TERM.strs[385])
#define backspace_if_not_bs              (_CUR_TERM.strs[386])
#define other_non_function_keys          (_CUR_TERM.strs[387])
#define arrow_key_map                    (_CUR_TERM.strs[388])
#define memory_lock_above                (_CUR_TERM.strs[389])
#define memory_unlock                    (_CUR_TERM.strs[390])
#define linefeed_if_not_lf               (_CUR_TERM.strs[391])
#define key_interrupt_char               (_CUR_TERM.strs[392])
#define key_kill_char                    (_CUR_TERM.strs[393])
#define key_suspend_char                 (_CUR_TERM.strs[394])
#define scroll_left                      (_CUR_TERM.strs[395])
#define scroll_right                     (_CUR_TERM.strs[396])
#define parm_scroll_left                 (_CUR_TERM.strs[397])
#define parm_scroll_right                (_CUR_TERM.strs[398])
#define _get_other                       (_CUR_TERM.strs[399])

#define NUM_OF_BOOLS	53
#define NUM_OF_NUMS	42
#define NUM_OF_STRS	400

#ifndef OVERRIDE
#undef _USE_SGTTY
#undef _USE_TERMIO
#undef _USE_TERMIOS
#define _USE_TERMIOS
#undef _USE_SMALLMEM
#undef _USE_PROTOTYPES
#define _USE_PROTOTYPES
#undef _USE_WINSZ
#define _USE_WINSZ
#undef _USE_TERMINFO
#undef _USE_TERMCAP
#define _USE_TERMCAP
#undef _MAX_CHUNK
#define _MAX_CHUNK 640
#endif /* OVERRIDE */

/* 
 * 92/02/01 07:30:28
 * @(#) mytinfo term.tail 3.2 92/02/01 public domain, By Ross Ridge
 *
 */

#if defined(_USE_TERMIO) || defined(_USE_TERMIOS)
#ifndef ICANON
#ifdef _USE_TERMIO
#include <termio.h>
#else
#include <termios.h>
#endif
#endif
#if defined(_USE_WINSZ) && defined(__FreeBSD__)
#include <sys/ioctl.h>
#endif
#if defined(_USE_WINSZ) && defined(xenix)
#include <sys/stream.h>
#include <sys/ptem.h>
#endif
#endif

#ifdef _USE_SGTTY
#ifndef CBREAK
#include <sgtty.h>
#endif
#endif

typedef struct _terminal {
	int fd;
#ifdef _USE_SMALLMEM
#ifdef _USE_TERMIOS
	speed_t baudrate;
#else
	unsigned short baudrate;
#endif
	unsigned pad:1, xon:1, termcap:1;
#else
	int pad;
	int xon;
	int termcap;
#ifdef _USE_TERMIOS
	speed_t baudrate;
#else
	long baudrate;
#endif
#endif
	char padch;
	short true_lines, true_columns;
	struct strbuf {
		struct strbuf *next;
#ifdef _USE_SMALLMEM
		short len;
#else
		int len;
#endif
		char buf[_MAX_CHUNK];
	} *strbuf;
	char *name, *name_long, *name_all;
#ifdef _USE_SGTTY
	struct sgtty_str {
		struct sgttyb v6;
#ifdef TIOCGETC
		struct tchars v7;
#endif
#ifdef TIOCLGET
		int bsd;
#endif
#ifdef TIOCGLTC
		struct ltchars bsd_new;
#endif
	} prog_mode, shell_mode;
#else	/* _USE_SGTTY */
#ifdef _USE_TERMIOS
	struct termios prog_mode, shell_mode;
#else
#ifdef _USE_TERMIO
	struct termio prog_mode, shell_mode;
#endif 
#endif 
#endif	/* else _USE_SGTTY */
#ifdef _USE_WINSZ
#ifdef TIOCGWINSZ
	struct winsize prog_winsz, shell_winsz;
#endif
#endif
	char bools[NUM_OF_BOOLS];
	short nums[NUM_OF_NUMS];
	char *strs[NUM_OF_STRS];
} TERMINAL;

#ifndef _CUR_TERM
#ifdef SINGLE
#define _CUR_TERM _term_buf
#else
#define _CUR_TERM (*cur_term)
#endif
#endif

extern TERMINAL *cur_term;
extern TERMINAL _term_buf;

#ifndef __P
#if defined(_USE_PROTOTYPES) && (defined(__STDC__) || defined(__cplusplus))
#define	__P(protos)	protos		/* full-blown ANSI C */
#else
#define	__P(protos)	()		/* traditional C preprocessor */
#endif
#endif

extern char *tparm __P((const char *, ...));
extern int setupterm __P((char *, int, int *)), set_curterm __P((TERMINAL *));
extern int del_curterm __P((TERMINAL *)), tputs __P((const char *, int, int (*)(int)));
extern int putp __P((char *));
extern int tigetflag __P((char *)), tigetnum __P((char *));
extern char *tigetstr __P((char *));
extern int def_prog_mode __P((void)), def_shell_mode __P((void));
extern int reset_prog_mode __P((void)), reset_shell_mode __P((void));

extern char *boolnames[], *boolcodes[], *boolfnames[];
extern char *numnames[], *numcodes[], *numfnames[];
extern char *strnames[], *strcodes[], *strfnames[];

#ifndef OK
#undef ERR
#define OK (0)
#define ERR (-1)
#endif

/* Compatibility */
#define Filedes fd
#define Ottyb shell_mode
#define Nttyb prog_mode
#define TTY struct termios

#endif /* _TERM_H_ */
