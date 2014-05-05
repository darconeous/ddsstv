INCLUDES =  -I.. -I"$(top_srcdir)/src" -I"$(top_srcdir)/src/pt-1.4" -I"$(top_srcdir)/src/utils"

bin_PROGRAMS = ddsstv-encoder ddsstv-decoder

ddsstv_encoder_SOURCES = \
	ddsstv-encoder-main.c \
	../utils/help.c

ddsstv_encoder_LDADD = \
	../dddsp/libdddsp.la \
	../ddsstv/libddsstv.la \
	@SDL_LIBS@

ddsstv_encoder_CFLAGS = @CFLAGS@ @SDL_CFLAGS@

ddsstv_decoder_SOURCES = \
	ddsstv-decoder-main.c \
	../utils/help.c

ddsstv_decoder_LDADD = \
	../dddsp/libdddsp.la \
	../ddsstv/libddsstv.la

ddsstv_decoder_CFLAGS = @CFLAGS@ @SDL_CFLAGS@

DISTCLEANFILES = .deps Makefile
