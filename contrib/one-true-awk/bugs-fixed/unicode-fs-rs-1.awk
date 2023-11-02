BEGIN {
	FS="␟"
	RS="␞"
	OFS=","
}
{ print $1, $2, $3 }
