okey_merge = {
	    key = value1;
	    key1 = value2;
}

akey_merge = ["value3"];

skey_merge = "value4";

# Escaped spellings of the keys above: the merged duplicate is freed while
# its key escape flag is still being recorded
"\u006fkey_merge_esc" = {
	    key1 = value2;
}

"\u0061key_merge_esc" = ["value3"];
