BEGIN {
	printf "Initially, PROCINFO[\"FS\"] = %s\n", PROCINFO["FS"]
	FIELDWIDTHS = "3 4 5 6"
	printf "After assign to FIELDWIDTHS, PROCINFO[\"FS\"] = %s\n", PROCINFO["FS"]
	FS = FS
	printf "After assign to FS, PROCINFO[\"FS\"] = %s\n", PROCINFO["FS"]
}
