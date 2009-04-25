# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(rsvndump, 0.5, jonas.gehring@boolsoft.org)
AM_INIT_AUTOMAKE()
AC_PREFIX_DEFAULT([/usr/local])
test $prefix = NONE && prefix=/usr/local

AC_CONFIG_HEADER([config.h])
AC_CONFIG_LIBOBJ_DIR([lib])

sinclude(build/find_apr.m4)
sinclude(build/find_svn.m4)
sinclude(build/rsvndump-conf.m4)

# Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_RANLIB

AM_GNU_GETTEXT([external])

# Checks for headers
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([limits.h termios.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([stdio_ext.h])
AC_CHECK_HEADERS([sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_FUNC_CLOSEDIR_VOID
AC_FUNC_MALLOC
AC_FUNC_FORK
AC_FUNC_FSEEKO
AC_CHECK_FUNCS([stat strchr strrchr strdup memchr], ,[AC_MSG_ERROR([Sorry, unable to compile this program without these functions])])
USE_COMPAT="no"
AC_CHECK_FUNCS([memset], ,[AC_LIBOBJ([memset]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([strdup], ,[AC_LIBOBJ([strdup]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([getpass],  ,[AC_LIBOBJ([getpass]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([rmdir], ,[AC_LIBOBJ([rmdir]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([strnlen], ,[AC_LIBOBJ([strnlen]) USE_COMPAT="yes"])
AC_CHECK_FUNCS([atexit strtol])
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
AC_CHECK_LIB([svn_wc-1], [svn_wc_ensure_adm2], ,[AC_MSG_ERROR([Neccessary Subversion libraries are missing])], [-L$SVN_PREFIX/lib])

AC_CONFIG_FILES([Makefile])
if test "$USE_COMPAT" = "yes"; then
  AC_CONFIG_FILES([lib/Makefile])
fi
AC_CONFIG_FILES([src/Makefile po/Makefile.in])
AC_OUTPUT
