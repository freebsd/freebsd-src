all: docs

%.eps: %.fig
	fig2dev -L eps $*.fig $*.eps

%.png: %.fig
	fig2dev -L png -m 3 $*.fig | pngtopnm | pnmscale 0.4 | pnmtopng \
		> $*.png

%.png: %.dot
	dot $*.dot -Tpng -o $*.png

%.eps: %.dot
	dot $*.dot -Tps -o $*.eps

_wpa_supplicant.png: wpa_supplicant.png
	cp $< $@

_wpa_supplicant.eps: wpa_supplicant.eps
	cp $< $@

docs-pics: wpa_supplicant.png wpa_supplicant.eps hostapd.png hostapd.eps p2p_sm.png p2p_sm.eps p2p_arch.png p2p_arch.eps p2p_arch2.png p2p_arch2.eps _wpa_supplicant.png _wpa_supplicant.eps

docs: docs-pics
	(cd ..; doxygen doc/doxygen.conf; cd doc)
	$(MAKE) -C latex
	cp latex/refman.pdf wpa_supplicant-devel.pdf

html: docs-pics
	(cd ..; doxygen doc/doxygen.conf; cd doc)

clean:
	rm -f *~
	rm -f wpa_supplicant.eps wpa_supplicant.png
	rm -f _wpa_supplicant.png _wpa_supplicant.eps
	rm -f hostapd.eps hostapd.png
	rm -f p2p_sm.eps p2p_sm.png
	rm -f p2p_arch.eps p2p_arch.png
	rm -f p2p_arch2.eps p2p_arch2.png
	rm -f doxygen.warnings
	rm -rf html latex
	rm -f wpa_supplicant-devel.pdf
