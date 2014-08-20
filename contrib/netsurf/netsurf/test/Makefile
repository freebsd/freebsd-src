CFLAGS := -std=c99 -g -O0 -D_BSD_SOURCE -D_POSIX_C_SOURCE -I.. \
		$(shell pkg-config --cflags libcurl)
LDFLAGS := $(shell pkg-config --libs libcurl) -lz

llcache_CFLAGS := $(shell pkg-config --cflags libparserutils libwapcaplet libdom) -O2
llcache_LDFLAGS := $(shell pkg-config --libs libparserutils libwapcaplet libdom)

llcache_SRCS := content/fetch.c content/fetchers/curl.c \
		content/fetchers/about.c content/fetchers/data.c \
		content/fetchers/resource.c content/llcache.c \
		content/urldb.c desktop/options.c desktop/version.c \
		image/image_cache.c \
		utils/base64.c utils/corestrings.c utils/hashtable.c \
		utils/log.c utils/nsurl.c utils/messages.c utils/url.c \
		utils/useragent.c utils/utils.c test/llcache.c

urldbtest_SRCS := content/urldb.c utils/url.c utils/utils.c utils/log.c \
		desktop/options.c utils/messages.c utils/hashtable.c \
		utils/filename.c utils/nsurl.c utils/corestrings.c \
		test/urldbtest.c

urldbtest_CFLAGS := $(shell pkg-config --cflags libwapcaplet libdom) -O2
urldbtest_LDFLAGS := $(shell pkg-config --libs libwapcaplet libdom)

nsurl_SRCS := utils/corestrings.c utils/log.c utils/nsurl.c test/nsurl.c
nsurl_CFLAGS := $(shell pkg-config --cflags libwapcaplet libdom)
nsurl_LDFLAGS := $(shell pkg-config --libs libwapcaplet libdom)

nsoption_SRCS := utils/log.c utils/nsoption.c test/nsoption.c
nsoption_CFLAGS := -Dnsgtk

.PHONY: all

all: llcache urldbtest nsurl nsoption

llcache: $(addprefix ../,$(llcache_SRCS))
	$(CC) $(CFLAGS) $(llcache_CFLAGS) $^ -o $@ $(LDFLAGS) $(llcache_LDFLAGS)

urldbtest: $(addprefix ../,$(urldbtest_SRCS))
	$(CC) $(CFLAGS) $(urldbtest_CFLAGS) $^ -o $@ $(LDFLAGS) $(urldbtest_LDFLAGS)

nsurl: $(addprefix ../,$(nsurl_SRCS))
	$(CC) $(CFLAGS) $(nsurl_CFLAGS) $^ -o $@ $(LDFLAGS) $(nsurl_LDFLAGS)

nsoption: $(addprefix ../,$(nsoption_SRCS))
	$(CC) $(CFLAGS) $(nsoption_CFLAGS) $^ -o $@ $(LDFLAGS) $(nsoption_LDFLAGS)

.PHONY: clean

clean:
	$(RM) llcache urldbtest nsurl nsoption
