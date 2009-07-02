bin_PROGRAMS = rsvndump_ut
rsvndump_ut_SOURCES = \
	utests.c \
	../../src/main.c \
	../../src/list.c

localedir = $(datadir)/locale
AM_LDFLAGS = $(APR_LIBS) $(SVN_LDFLAGS) $(COMPAT_LIBS) $(LIB_INTL)
AM_CFLAGS = -I../../src -DUNIT_TESTS $(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) $(SVN_CFLAGS) -Wall -Wno-deprecated-declarations
