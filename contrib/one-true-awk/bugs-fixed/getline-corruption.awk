BEGIN { 
	getline l
	getline l
	print (s=substr(l,1,10)) " len=" length(s)
}
