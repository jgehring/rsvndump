noinst_LIBRARIES = libcompat.a

libcompat_a_SOURCES = \
	critbit/critbit.h critbit/critbit.c \
	\
	snappy-c/compat.h \
	snappy-c/snappy-int.h \
	snappy-c/snappy.h snappy-c/snappy.c


libcompat_a_LIBADD = $(LIBOBJS) $(ALLOCA)

EXTRA_DIST = \
	critbit/README \
	critbit/critbit.w \
	snappy-c/README \
	snappy-c/LICENSE
