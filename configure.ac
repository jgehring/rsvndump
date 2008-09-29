# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(rsvndump, 0.4, jonas.gehring@boolsoft.org)
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

# Checks for headers
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([limits.h termios.h])
AC_CHECK_HEADERS([sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_CHAR_UNSIGNED

AC_FUNC_CLOSEDIR_VOID
AC_FUNC_MALLOC
AC_CHECK_FUNCS([stat strchr strrchr strdup], ,[AC_MSG_ERROR([Sorry, unable to compile this program without this functions])])
AC_CHECK_FUNCS([memset], ,[AC_LIBOBJ([memset])])
AC_CHECK_FUNCS([strdup], ,[AC_LIBOBJ([strdup])])
AC_CHECK_FUNCS([getpass], ,[AC_LIBOBJ([getpass])])
AC_CHECK_FUNCS([rmdir], ,[AC_LIBOBJ([rmdir])])
AC_CHECK_FUNCS([gettimeofday fopen64])

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
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([lib/Makefile])
AC_OUTPUT
