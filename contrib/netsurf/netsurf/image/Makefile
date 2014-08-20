# Image content handlers sources

# S_IMAGE are sources related to image management
S_IMAGE_YES := image.c image_cache.c
S_IMAGE_NO :=
S_IMAGE_$(NETSURF_USE_BMP) += bmp.c ico.c
S_IMAGE_$(NETSURF_USE_GIF) += gif.c
S_IMAGE_$(NETSURF_USE_JPEG) += jpeg.c
S_IMAGE_$(NETSURF_USE_ROSPRITE) += nssprite.c
S_IMAGE_$(NETSURF_USE_PNG) += png.c
S_IMAGE_$(NETSURF_USE_NSSVG) += svg.c
S_IMAGE_$(NETSURF_USE_RSVG) += rsvg.c
S_IMAGE_$(NETSURF_USE_WEBP) += webp.c
S_IMAGE_$(NETSURF_USE_VIDEO) += video.c

S_IMAGE := $(addprefix image/,$(S_IMAGE_YES))
