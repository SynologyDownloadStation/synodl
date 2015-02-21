bin_PROGRAMS = synodl

synodl_SOURCES = synodl.c cfg.c cfg.h syno.c syno.h ini.c ini.h ui.c ui.h task.h
synodl_LDADD = $(libjson_LIBS) $(libcurl_LIBS) $(libncursew_LIBS) -lm
synodl_CFLAGS = $(libjson_CFLAGS) $(libcurl_CFLAGS) $(libncursew_CFLAGS)
