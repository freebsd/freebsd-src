proc simple_principal {name} {
    return "{$name} 0 0 0 0 {$name} 0 0 0 0 null 0"
}

proc princ_w_pol {name policy} {
    return "{$name} 0 0 0 0 {$name} 0 0 0 0 {$policy} 0"
}

proc simple_policy {name} {
    return "{$name} 0 0 0 0 0 0 0 0 0"
}

proc config_params {masks values} {
    if {[llength $masks] != [llength $values]} {
	error "config_params: length of mask and values differ"
    }

    set params [list $masks 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 {}]
    for {set i 0} {$i < [llength $masks]} {incr i} {
	set mask [lindex $masks $i]
	set value [lindex $values $i]
	switch -glob -- $mask {
	    "KADM5_CONFIG_REALM" {set params [lreplace $params 1 1 $value]}
	    "KADM5_CONFIG_KADMIND_PORT" {
		set params [lreplace $params 2 2 $value]}
	    "KADM5_CONFIG_ADMIN_SERVER" {
		set params [lreplace $params 3 3 $value]}
	    "KADM5_CONFIG_DBNAME" {set params [lreplace $params 4 4 $value]}
	    "KADM5_CONFIG_ADBNAME" {set params [lreplace $params 5 5 $value]}
	    "KADM5_CONFIG_ADB_LOCKFILE" {
		set params [lreplace $params 6 6 $value]}
	    "KADM5_CONFIG_ACL_FILE" {set params [lreplace $params 8 8 $value]}
	    "KADM5_CONFIG_DICT_FILE" {
		set params [lreplace $params 9 9 $value]}
	    "KADM5_CONFIG_MKEY_FROM_KBD" {
		set params [lreplace $params 10 10 $value]}
	    "KADM5_CONFIG_STASH_FILE" {
		set params [lreplace $params 11 11 $value]}
	    "KADM5_CONFIG_MKEY_NAME" {
		set params [lreplace $params 12 12 $value]}
	    "KADM5_CONFIG_ENCTYPE" {set params [lreplace $params 13 13 $value]}
	    "KADM5_CONFIG_MAX_LIFE" {
		set params [lreplace $params 14 14 $value]}
	    "KADM5_CONFIG_MAX_RLIFE" {
		set params [lreplace $params 15 15 $value]}
	    "KADM5_CONFIG_EXPIRATION" {
		set params [lreplace $params 16 16 $value]}
	    "KADM5_CONFIG_FLAGS" {set params [lreplace $params 17 17 $value]}
	    "KADM5_CONFIG_ENCTYPES" {
		set params [lreplace $params 18 19 [llength $value] $value]}
	    "*" {error "config_params: unknown mask $mask"}
	}
    }
    return $params
}

	    

