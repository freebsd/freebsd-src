BEGIN {
	FS = 0; split("20202", a); print a[1];
	FS = 1; $0="31313"; print $1;
	FS = 2; "echo 42424" | getline; print $1;
}
