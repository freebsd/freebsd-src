# The full test will only work in a Swedish localte
# Try things that should work across the board
# BEGIN {
# 	s = "så är det"
# 	print match(s,/\yså\y/), s ~ /\yså\y/, "å" ~ /\w/
# }
BEGIN {
	printf "\"å\" = %c\n", "å"
	printf "\"ä\" = %c\n", "ä"
	s = "så är det"
	printf "s = \"%s\"\n", s
	printf "match(s,/\\yså/) = %d\n", match(s, /\yså/)
# 	printf "match(s,/så\\y/) = %d\n", match(s, /så\y/)
# 	printf "match(s,/\\yså\\y/) = %d\n", match(s, /\yså\y/)
	printf "s ~ /å/ = %d\n", s ~ /å/
	printf "s ~ /så/ = %d\n", s ~ /så/
	printf "s ~ /\\yså/ = %d\n", s ~ /\yså/
# 	printf "s ~ /så\\y/ = %d\n", s ~ /så\y/
# 	printf "s ~ /\\yså\\y/ = %d\n", s ~ /\yså\y/
# 	printf "\"å\" ~ /\\w/ = %d\n", "å" ~ /\w/
# 	printf "\"ä\" ~ /\\w/ = %d\n", "ä" ~ /\w/
# 	printf "\"å\" ~ /\\yä\\y/ = %d\n", "å" ~ /\yå\y/
# 	printf "\"ä\" ~ /\\yä\\y/ = %d\n", "ä" ~ /\yä\y/
# 	printf "\"å\" ~ /[[:alpha:]]/ = %d\n", "å" ~ /[[:alpha:]]/
# 	printf "\"ä\" ~ /[[:alpha:]]/ = %d\n", "ä" ~ /[[:alpha:]]/
}
