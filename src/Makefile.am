bin_PROGRAMS = rsvndump
rsvndump_SOURCES = main.c main.h list.c list.h node.c node.h property.c property.h dump.c dump.h utils.c utils.h \
		   wsvn.c wsvn.h logentry.c logentry.h whash.c whash.h gettext.h

localedir = $(datadir)/locale
AM_LDFLAGS = $(APR_LIBS) $(SVN_LDFLAGS) $(COMPAT_LIBS) $(LIB_INTL)
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" $(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) $(SVN_CFLAGS) 
