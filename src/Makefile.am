LDADD = ../lib/libcompat.a
bin_PROGRAMS = rsvndump
rsvndump_SOURCES = main.c main.h list.c list.h node.c node.h property.c property.h dump.c dump.h utils.c utils.h \
		   wsvn.c wsvn.h logentry.c logentry.h whash.c whash.h

AM_LDFLAGS = $(APR_LIBS) $(SVN_LDFLAGS)
AM_CFLAGS = $(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) $(SVN_CFLAGS)
