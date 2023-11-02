BEGIN {
	FS = "א"
	RS = "בב"
	OFS = ","
}

{ print $1, $2, $3 }
