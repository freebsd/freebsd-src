JUNKFILES = tip-index.ps tip-index.dvi tip-index.tex tip-index.log \
	tip-index.aux

tip-index.ps : tip-index.dvi
	dvips tip-index.dvi

tip-index.dvi : tip-index.tex
	latex tip-index.tex

tip-index.tex : tip.texified header.tex footer.tex
	cat header.tex tip.texified footer.tex > tip-index.tex

tip.texified : tip.eign tip.forgptx Makefile
	gptx -f -r -i ./tip.eign -T < tip.forgptx | x.pl > tip.texified

tip.eign : /usr/lib/eign exclude-words
	cat /usr/lib/eign exclude-words > tip.eign

screenlist : tip.texified
	cat tip.texified \
	| gawk -F\{ '{count[$$4]++} \
		END {for (s in count) printf("%d %20s\n", count[s], s)}' \
		| tr -d '}' \
		| sort -n > screenlist
	@echo "Check (say) the last 100 lines of ./screenlist".

clean :
	rm -f tip.eign tip.texified $(JUNKFILES) screenlist
