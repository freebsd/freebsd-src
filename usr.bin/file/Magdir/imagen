# Tell file about magic for IMAGEN printer-ready files:
0	string	@document(		Imagen printer
# this only works if "language xxx" is first item in Imagen header.
>10	string	language\ impress	(imPRESS data)
>10	string	language\ daisy		(daisywheel text)
>10	string	language\ diablo		(daisywheel text)
>10	string	language\ printer	(line printer emulation)
>10	string	language\ tektronix	(Tektronix 4014 emulation)
# Add any other languages that your Imagen uses - remember
# to keep the word `text' if the file is human-readable.
#
# Now magic for IMAGEN font files...
0	string		Rast		RST-format raster font data
>45	string		>0		face %
