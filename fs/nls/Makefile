#
# Makefile for native language support
#

obj-y	:=	nls_base.o
obj-m	:=
obj-n	:=
obj-	:=

obj-$(CONFIG_NLS_CODEPAGE_437)	+= nls_cp437.o
obj-$(CONFIG_NLS_CODEPAGE_737)	+= nls_cp737.o
obj-$(CONFIG_NLS_CODEPAGE_775)	+= nls_cp775.o
obj-$(CONFIG_NLS_CODEPAGE_850)	+= nls_cp850.o
obj-$(CONFIG_NLS_CODEPAGE_852)	+= nls_cp852.o
obj-$(CONFIG_NLS_CODEPAGE_855)	+= nls_cp855.o
obj-$(CONFIG_NLS_CODEPAGE_857)	+= nls_cp857.o
obj-$(CONFIG_NLS_CODEPAGE_860)	+= nls_cp860.o
obj-$(CONFIG_NLS_CODEPAGE_861)	+= nls_cp861.o
obj-$(CONFIG_NLS_CODEPAGE_862)	+= nls_cp862.o
obj-$(CONFIG_NLS_CODEPAGE_863)	+= nls_cp863.o
obj-$(CONFIG_NLS_CODEPAGE_864)	+= nls_cp864.o
obj-$(CONFIG_NLS_CODEPAGE_865)	+= nls_cp865.o
obj-$(CONFIG_NLS_CODEPAGE_866)	+= nls_cp866.o
obj-$(CONFIG_NLS_CODEPAGE_869)	+= nls_cp869.o
obj-$(CONFIG_NLS_CODEPAGE_874)	+= nls_cp874.o nls_tis-620.o
obj-$(CONFIG_NLS_CODEPAGE_932)	+= nls_cp932.o nls_sjis.o nls_euc-jp.o
obj-$(CONFIG_NLS_CODEPAGE_936)	+= nls_cp936.o nls_gb2312.o
obj-$(CONFIG_NLS_CODEPAGE_949)	+= nls_cp949.o nls_euc-kr.o
obj-$(CONFIG_NLS_CODEPAGE_950)	+= nls_cp950.o nls_big5.o
obj-$(CONFIG_NLS_CODEPAGE_1250)	+= nls_cp1250.o
obj-$(CONFIG_NLS_CODEPAGE_1251)	+= nls_cp1251.o
obj-$(CONFIG_NLS_ISO8859_1)	+= nls_iso8859-1.o
obj-$(CONFIG_NLS_ISO8859_2)	+= nls_iso8859-2.o
obj-$(CONFIG_NLS_ISO8859_3)	+= nls_iso8859-3.o
obj-$(CONFIG_NLS_ISO8859_4)	+= nls_iso8859-4.o
obj-$(CONFIG_NLS_ISO8859_5)	+= nls_iso8859-5.o
obj-$(CONFIG_NLS_ISO8859_6)	+= nls_iso8859-6.o
obj-$(CONFIG_NLS_ISO8859_7)	+= nls_iso8859-7.o
obj-$(CONFIG_NLS_ISO8859_8)	+= nls_cp1255.o nls_iso8859-8.o
obj-$(CONFIG_NLS_ISO8859_9)	+= nls_iso8859-9.o
obj-$(CONFIG_NLS_ISO8859_13)	+= nls_iso8859-13.o
obj-$(CONFIG_NLS_ISO8859_14)	+= nls_iso8859-14.o
obj-$(CONFIG_NLS_ISO8859_15)	+= nls_iso8859-15.o
obj-$(CONFIG_NLS_KOI8_R)	+= nls_koi8-r.o
obj-$(CONFIG_NLS_KOI8_U)	+= nls_koi8-u.o nls_koi8-ru.o
obj-$(CONFIG_NLS_UTF8)		+= nls_utf8.o

export-objs = $(obj-y)

O_TARGET = nls.o

include $(TOPDIR)/Rules.make
