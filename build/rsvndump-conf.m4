dnl
dnl custom autoconf rules for rsvndump
dnl

dnl
dnl RSVND_FIND_APR: figure out where APR is located
dnl
dnl This method is taken from the apr-util package
dnl
AC_DEFUN([RSVND_FIND_APR], [

	dnl use the find_apr.m4 script to locate APR. sets apr_found and apr_config
	APR_FIND_APR(,,[1],[1])
	if test "$apr_found" = "no"; then
		AC_MSG_ERROR([APR could not be located. Please use the --with-apr option.])
	fi

	APR_BUILD_DIR="`$apr_config --installbuilddir`"

	dnl make APR_BUILD_DIR an absolute directory (we'll need it in the
	dnl sub-projects in some cases)
	APR_BUILD_DIR="`cd $APR_BUILD_DIR && pwd`"

	APR_CFLAGS=`$apr_config --cflags`
	APR_CPPFLAGS=`$apr_config --cppflags`
	APR_INCLUDES="`$apr_config --includes`"
	APR_LIBS="`$apr_config --link-ld --libs`"

	AC_SUBST(APR_INCLUDES)
	AC_SUBST(APR_LIBS)
])

dnl
dnl RSVND_FIND_SVN: locate subversion include files
dnl 
AC_DEFUN([RSVND_FIND_SVN], [

	FIND_SVN()

	if test "$svn_found" != "yes"; then
		AC_MSG_ERROR([Subversion could not be located. Please use the --with-svn option.])
	fi

	if test "$svn_major" != "1" || test $svn_minor -lt 4; then
		AC_MSG_ERROR([Subversion >= 1.4 is needed. Please upgrade your installtion])
	fi

	AC_SUBST(SVN_CFLAGS)
	AC_SUBST(SVN_LDFLAGS)
])
