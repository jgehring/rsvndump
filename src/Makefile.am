bin_PROGRAMS = rsvndump
rsvndump_SOURCES = \
	delta.c delta.h \
	dump.c dump.h \
	log.c log.h \
	logger.c logger.h \
	main.c main.h \
	mukv.c mukv.h \
	path_repo.c path_repo.h \
	property.c property.h \
	rhash.c rhash.h \
	session.c session.h \
	utils.c utils.h

localedir = $(datadir)/locale
AM_LDFLAGS = $(SVN_LDFLAGS)
LIBS += $(top_builddir)/lib/libcompat.a $(APR_LIBS) $(APU_LIBS) $(GDBM_LIBS) $(LIBINTL) 
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" \
	$(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) \
	$(APU_INCLUDES) \
	$(SVN_CFLAGS) \
	$(GDBM_INCLUDES) \
	-Wall -Wno-deprecated-declarations -Wsign-compare \
	-pedantic -ansi \
	-I$(top_srcdir)/lib
