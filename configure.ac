# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(rsvndump, 0.5.7, jonas.gehring@boolsoft.org)
AM_INIT_AUTOMAKE()
AC_PREFIX_DEFAULT([/usr/local])
test $prefix = NONE && prefix=/usr/local

AC_CONFIG_HEADER([config.h])
AC_CONFIG_LIBOBJ_DIR([lib])

sinclude(m4/find_apr.m4)
sinclude(m4/find_apu.m4)
sinclude(m4/find_svn.m4)
sinclude(m4/rsvndump_conf.m4)

# Optional features
USE_MAN="no"
AC_ARG_ENABLE(man, [  --enable-man            generate the man page], USE_MAN="$enableval", USE_MAN="no")
AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug], [Turn on debugging])], [debug="$enableval"], [debug="no"])
AM_CONDITIONAL([DEBUG], [test "x$debug" = "xyes"])
if test "x$debug" = "xyes"; then
	CXXFLAGS="${CXXFLAGS} -O0 -g -DDEBUG"
	CFLAGS="${CFLAGS} -O0 -g -DDEBUG"
else
	CXXFLAGS="-O2 ${CXXFLAGS} -DNDEBUG"
	CFLAGS="-O2 ${CFLAGS} -DNDEBUG"
fi

# Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_RANLIB
if test "$USE_MAN" = "yes"; then
	RSVN_CHECK_MAN_PROGS
fi

AM_GNU_GETTEXT([external])

# Checks for headers
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h locale.h])
AC_CHECK_HEADERS([sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_STRUCT_TM

AC_FUNC_MALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strchr], ,[AC_MSG_ERROR([Sorry, unable to compile this program without these functions])])
USE_COMPAT="no"
AC_CHECK_FUNCS([memset], ,[AC_LIBOBJ([memset]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([memcmp], ,[AC_LIBOBJ([memcmp]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([strdup], ,[AC_LIBOBJ([strdup]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([setlocale], , USE_NLS="no")
AC_CHECK_FUNCS([atexit strtol gettimeofday])
if test "$USE_COMPAT" = "yes"; then
	COMPAT_LIBS="../lib/libcompat.a"
	COMPAT_SUBDIRS="lib"
	AC_SUBST([COMPAT_LIBS])
	AC_SUBST([COMPAT_SUBDIRS])
fi

# Checks for libraries
RSVND_FIND_APR

SCFLAGS="$CFLAGS"
SCPPFLAGS="$CPPFLAGS"
CFLAGS="$CFLAGS $APR_INCLUDES $APR_CFLAGS"
CPPFLAGS="$CPPFLAGS $APR_INCLUDES $APR_CPPFLAGS"

RSVND_FIND_SVN

CFLAGS="$SCFLAGS"
CPPFLAGS="$SCPPFLAGS"
AC_SUBST([APR_CPPFLAGS])
AC_SUBST([APR_CFLAGS])

# Checks for library functions.
AC_CHECK_LIB([svn_fs-1], [svn_fs_initialize], ,[AC_MSG_ERROR([Neccessary Subversion libraries are missing])], [-L$SVN_PREFIX/lib]) 
AC_CHECK_LIB([svn_client-1], [svn_client_open_ra_session], ,[AC_MSG_ERROR([Neccessary Subversion libraries are missing])], [-L$SVN_PREFIX/lib]) 
AC_CHECK_LIB([svn_ra-1], [svn_ra_initialize], ,[AC_MSG_ERROR([Neccessary Subversion libraries are missing])], [-L$SVN_PREFIX/lib]) 
AC_CHECK_LIB([svn_subr-1], [svn_auth_open], ,[AC_MSG_ERROR([Neccessary Subversion libraries are missing])], [-L$SVN_PREFIX/lib])
AC_CHECK_LIB([svn_delta-1], [svn_txdelta_apply], ,[AC_MSG_ERROR([Neccessary Subversion libraries are missing])], [-L$SVN_PREFIX/lib])

AC_CONFIG_FILES([Makefile])
if test "$USE_COMPAT" = "yes"; then
	AC_CONFIG_FILES([lib/Makefile])
fi
AC_CONFIG_FILES([src/Makefile po/Makefile.in])
if test "$USE_MAN" = "yes"; then
	DOC_SUBDIRS="doc"
	AC_SUBST([DOC_SUBDIRS])
	AC_CONFIG_FILES([doc/Makefile])
fi
AC_OUTPUT
