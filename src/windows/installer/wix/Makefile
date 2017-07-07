
# Build language
LANG=1033

# Program macros
CANDLE=candle -nologo

LIGHT=light -nologo

CD=cd

RM=del

MAKE=nmake -nologo


# Targets

OUTPATH=.

OBJFILE=$(OUTPATH)\kfw.wixobj

MSIFILE=$(OUTPATH)\kfw.msi

WIXINCLUDES= \
	config.wxi \
	features.wxi \
	files.wxi \
	property.wxi \
	runtime.wxi \
	site-local.wxi \
	lang\strings_$(LANG).wxl \
	lang\ui_$(LANG).wxi \
	lang\config_$(LANG).wxi

CUSTOMDLL=custom\custom.dll

all: $(MSIFILE)

$(OBJFILE): kfw.wxs $(WIXINCLUDES)
	$(CANDLE) -out $@ kfw.wxs \
		"-dDate=%DATE%" \
		"-dTime=%TIME%" \
		-dBuildLang=$(LANG)

$(MSIFILE): $(OBJFILE) $(CUSTOMDLL)
	$(LIGHT) -out $@ $(OBJFILE) \
		-loc lang\strings_$(LANG).wxl -ext WixUtilExtension.dll

$(CUSTOMDLL): custom\custom.cpp
	$(CD) custom
	$(MAKE) -f custom.cpp
	$(CD) ..

clean:
	$(RM) $(OBJFILE)
	$(RM) $(MSIFILE)
	$(CD) custom
	$(MAKE) -f custom.cpp clean
	$(CD) ..
