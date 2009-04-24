bin_PROGRAMS = rsvndump
rsvndump_SOURCES = \
	dump.c dump.h \
	dump_delta.c dump_delta.h \
	list.c list.h \
	logentry.c logentry.h \
	main.c main.h \
	node.c node.h \
	property.c property.h \
	utils.c utils.h \
	whash.c whash.h \
	wsvn.c wsvn.h

localedir = $(datadir)/locale
AM_LDFLAGS = $(APR_LIBS) $(SVN_LDFLAGS) $(COMPAT_LIBS) $(LIB_INTL)
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" $(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) $(SVN_CFLAGS) 
