bin_PROGRAMS = rsvndump
rsvndump_SOURCES = \
	delta.c delta.h \
	dump.c dump.h \
	list.c list.h \
	log.c log.h \
	logger.c logger.h \
	main.c main.h \
	path_hash.c path_hash.h \
	property.c property.h \
	rhash.c rhash.h \
	session.c session.h \
	utils.c utils.h


localedir = $(datadir)/locale
AM_LDFLAGS = $(APR_LIBS) $(SVN_LDFLAGS) $(COMPAT_LIBS) $(LIBINTL)
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" $(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) $(SVN_CFLAGS) -Wall -Wno-deprecated-declarations
