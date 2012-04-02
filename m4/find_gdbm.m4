dnl
dnl Checks for GDBM
dnl This function is mostly taken from apr-util
dnl
AC_DEFUN([FIND_GDBM], [
	gdbm_found=0
	AC_ARG_WITH([gdbm], [AC_HELP_STRING([--with-gdbm=PATH], [prefix for GNU dbm installation])],
	[
		if test "$withval" = "no" || test "$withval" = "yes"; then
			AC_MSG_ERROR([--with-gdbm requires a directory or file to be provided])
		else
			saved_cppflags="$CPPFLAGS"
			saved_ldflags="$LDFLAGS"
			CPPFLAGS="$CPPFLAGS -I$withval/include"
			LDFLAGS="$LDFLAGS -L$withval/lib "

			AC_MSG_CHECKING([checking for gdbm in $withval])
			AC_CHECK_HEADER(gdbm.h, AC_CHECK_LIB(gdbm, gdbm_open, [gdbm_found=1]))
			if test "$gdbm_found" != "0"; then
				AC_SUBST(GDBM_LDFLAGS, [-L$withval/lib])
				AC_SUBST(GDBM_INCLUDES, [-I$withval/include])
			fi
			CPPFLAGS="$saved_cppflags"
			LDFLAGS="$saved_ldflags"
		fi   
	],
	[
		AC_CHECK_HEADER(gdbm.h, AC_CHECK_LIB(gdbm, gdbm_open, [gdbm_found=1]))
	])

	if test "$gdbm_found" != "1"; then
		AC_MSG_ERROR([GNU dbm (GDBM) is needed.])
	fi
])
