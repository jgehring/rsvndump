bin_PROGRAMS = rsvndump
rsvndump_SOURCES = \
	delta.c delta.h \
	dump.c dump.h \
	log.c log.h \
	logger.c logger.h \
	main.c main.h \
	path_repo.c path_repo.h \
	property.c property.h \
	rhash.c rhash.h \
	session.c session.h \
	utils.c utils.h

rsvndump_SOURCES += \
	3rdparty/critbit/critbit.h \
	3rdparty/critbit/critbit.c \
	3rdparty/critbit/critbit.w


localedir = $(datadir)/locale
AM_LDFLAGS = $(APR_LIBS) $(APU_LIBS) $(SVN_LDFLAGS) $(COMPAT_LIBS) $(LIBINTL)
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" \
	$(APR_CFLAGS) $(APR_CPPFLAGS) $(APR_INCLUDES) \
	$(APU_INCLUDES) \
	$(SVN_CFLAGS) \
	-Wall -Wno-deprecated-declarations


EXTRA_DIST = \
	3rdparty/critbit/README
