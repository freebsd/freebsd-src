
BEGIN {
	FS = "[ \t\.\"]+"
}

{
	if ($1 ~ /^#define$/ && $2 ~ /^VERSION$/) {
		printf("%s.%s.%s\n", $3, $4, $5);
	}
}
