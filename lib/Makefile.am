AUTOMAKE_OPTIONS = subdir-objects

noinst_LIBRARIES = libcompat.a

libcompat_a_SOURCES = \
	critbit89/critbit.h critbit89/critbit.c \
	\
	snappy-c/compat.h \
	snappy-c/snappy-int.h \
	snappy-c/snappy.h snappy-c/snappy.c


libcompat_a_LIBADD = $(LIBOBJS) $(ALLOCA)

EXTRA_DIST = \
	critbit89/README \
	snappy-c/README \
	snappy-c/LICENSE
