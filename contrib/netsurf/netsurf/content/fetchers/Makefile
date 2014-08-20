# Content fetchers sources

S_FETCHERS := curl.c data.c file.c about.c resource.c

S_FETCHERS := $(addprefix content/fetchers/,$(S_FETCHERS))

# The following files depend on the testament
content/fetchers/about.c: testament $(OBJROOT)/testament.h
