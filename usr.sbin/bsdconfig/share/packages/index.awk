function _asorti(src, dest)
{
	k = nitems = 0

	# Copy src indices to dest and calculate array length
	for (i in src) dest[++nitems] = i

	# Sort the array of indices (dest) using insertion sort method
	for (i = 1; i <= nitems; k = i++)
	{
		idx = dest[i]
		while ((k > 0) && (dest[k] > idx))
		{
			dest[k+1] = dest[k]
			k--
		}
		dest[k+1] = idx
	}

	return nitems
}

function print_category(category, npkgs, desc)
{
	cat = category
	# Accent the category if the first page has been
	# cached (also acting as a visitation indicator)
	if ( ENVIRON["_index_page_" varcat "_1"] )
		cat = cat "*"
	printf "'\''%s'\'' '\''%s " packages "'\'' '\''%s'\''\n",
	       cat, npkgs, desc >>tmpfile
}

BEGIN{
	cnt=0
	div=int(npkg / 100)
	last=0
	prefix = ""
}
{
	cnt+=1
	i = int(cnt / div)
	if (i > last) {
		last = i
		print "XXX"
		print i
		print msg
		print "XXX"
		fflush("/dev/stdout");
	}
	varpkg = $1
	gsub("[^" valid_chars "]", "_", varpkg)
	print "_categories_" varpkg "=\"" $7 "\"" >> tmpfile
	split($7, pkg_categories, /[[:space:]]+/)
	for (pkg_category in pkg_categories)
		categories[pkg_categories[pkg_category]]++
	print "_rundeps_" varpkg "=\"" $9 "\"" >> tmpfile

}
END {
	n = _asorti(categories, categories_sorted)
	# Produce package counts for each category
	for (i = 1; i <= n; i++)
	{
		cat = varcat = categories_sorted[i]
		npkgs = categories[cat]
		gsub("[^" valid_chars "]", "_", varcat)
		print "_npkgs_" varcat "=\"" npkgs "\"" >>tmpfile
	}
	#
	# Create menu list and generate list of categories at same time
	print "CATEGORY_MENU_LIST=\"" >>tmpfile
	print_category(msg_all, npkg, msg_all_desc)
	category_list = ""
	for (i = 1; i <= n; i++)
	{
		cat = varcat = categories_sorted[i]
		npkgs = categories[cat]
		cur_prefix = tolower(substr(cat, 1, 1))
		if ( prefix != cur_prefix )
			prefix = cur_prefix
		else
			cat = " " cat
		gsub("[^" valid_chars "]", "_", varcat)
		desc = ENVIRON["_category_" varcat]
		if ( ! desc ) desc = default_desc
		print_category(cat, npkgs, desc)
		category_list = category_list " " cat
	}
	print "\"" >>tmpfile

	# Produce the list of categories (calculated in above block)
	sub(/^ /, "", category_list)
	print "PACKAGE_CATEGORIES=\"" category_list "\"" >> tmpfile
	print "_npkgs=\""npkg"\"" >>tmpfile

	print "EOF"
}
