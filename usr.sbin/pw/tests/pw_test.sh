atf_test_case R_missing_dir
R_missing_dir_body() {
        atf_check -s exit:64 -e inline:"pw: -R requires a directory argument\n" pw -R
}

atf_test_case V_missing_dir
V_missing_dir_body() {
        atf_check -s exit:64 -e inline:"pw: -V requires a directory argument\n" pw -V
}

atf_test_case M_missing_file
M_missing_file_body() {
        atf_check -s exit:64 -e inline:"pw: -M requires a file argument\n" pw -M
}

atf_init_test_cases() {
	atf_add_test_case R_missing_dir
	atf_add_test_case V_missing_dir
	atf_add_test_case M_missing_file
}
