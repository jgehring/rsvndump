bin_PROGRAMS = rsvndump
rsvndump_SOURCES = \
	dump.c dump.h \
	list.c list.h \
	log.c log.h \
	main.c main.h \
	session.c session.h \
	utils.c utils.h


localedir = $(datadir)/locale
AM_LDFLAGS = $(APR_LIBS) $(SVN_LDFLAGS) $(COMPAT_LIBS) $(LIB_INTL)
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" $(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) $(SVN_CFLAGS) 
