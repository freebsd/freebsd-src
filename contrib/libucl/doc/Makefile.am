EXTRA_DIST = api.md

dist_man_MANS = libucl.3

gen-man: @PANDOC@
	tail -n +$$(grep -n '# Synopsis' api.md | cut -d':' -f1) api.md | \
	cat pandoc.template - | sed -e 's/^# \(.*\)/# \U\1/' | \
	@PANDOC@ -s -f markdown -t man -o libucl.3 
