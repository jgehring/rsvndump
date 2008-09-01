# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(rsvndump, 0.3.1, jonas.gehring@boolsoft.org)
AM_INIT_AUTOMAKE()
AC_PREFIX_DEFAULT([/usr/local])
test $prefix = NONE && prefix=/usr/local

AC_CONFIG_HEADER([config.h])

AC_DEFUN([SVN_CHECK_HEADER], [
			    AC_MSG_CHECKING([for subversion-1 include directory])
			    SVN_FOUND="yes"
			    SVN_CFLAGS="-I/usr/include/subversion-1"
			    find /usr/include/subversion-1 -prune -fprint /dev/stderr 2> /dev/null
			    if (test  "$?" != "0"); then  
				    SVN_CFLAGS="-I/usr/local/include/subversion-1"
				    find /usr/local/include/subversion-1 -prune -fprint /dev/stderr 2> /dev/null
				    if (test  "$?" != "0"); then  
					    AC_MSG_RESULT([no])
					    SVN_FOUND="no"
				    fi
			    fi
			    AC_MSG_RESULT([yes])
			    CFLAGS="$CFLAGS $SVN_CFLAGS"
			    CPPFLAGS="$CPPFLAGS $SVN_CFLAGS"
			    AC_CHECK_HEADERS([svn_types.h], , SVN_FOUND="no")
			    ])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES(APR, apr-1)
SCFLAGS="$CFLAGS"
SCPPFLAGS="$CPPFLAGS"
CFLAGS="$CFLAGS $APR_CFLAGS"
CPPFLAGS="$CPPFLAGS $APR_CFLAGS"
#AC_ARG_WITH(svn, [AS_HELP_STRING([--with-svn=DIR],[prefix for subversion API files [[PREFIX]]])], [ LDFLAGS="$LDFLAGS -L$withval/lib"  SVN_CFLAGS="-I$withval/include/subversion-1"])
SVN_CHECK_HEADER()
if (test "$SVN_FOUND" = "no"); then
	AC_MSG_ERROR([The subversion API was not found.])
fi
CFLAGS="$SCFLAGS"
CPPFLAGS="$SCPPFLAGS"
AC_SUBST([SVN_CFLAGS])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([getpass strdup strrchr])
AC_SEARCH_LIBS([svn_client_cat2], [svn_client-1], SVN_LIBS="-lsvn_client-1")
if (test -z "$SVN_LIBS"); then
	AC_MSG_ERROR([The subversion API (libsvn_client-1) does not contain at least  one function that is needed.
Please check that you have got the latest subversion API])
fi
AC_SUBST([SVN_LIBS])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT
