dnl
dnl custom autoconf rules for rsvndump
dnl

dnl
dnl RSVND_FIND_APR: figure out where APR and APU is located
dnl
dnl Parts of this method are taken from the apr-util package
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

	AC_SUBST(APR_CFLAGS)
	AC_SUBST(APR_CPPFLAGS)
	AC_SUBST(APR_INCLUDES)
	AC_SUBST(APR_LIBS)

	APR_FIND_APU(,,[1],[1])
	if test "$apu_found" = "no"; then
		AC_MSG_ERROR([APR-UTIL could not be located. Please use the --with-apr-util option.])
	fi

	APU_INCLUDES="`$apu_config --includes`"
	APU_LIBS="`$apu_config --link-ld --libs`"

	AC_SUBST(APU_INCLUDES)
	AC_SUBST(APU_LIBS)
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


dnl
dnl RSVN_CHECK_MAN_PROGS: Checks for programs needed for man page generation
dnl
AC_DEFUN([RSVN_CHECK_MAN_PROGS], [
	
	dnl Check for Asciidoc
	AC_ARG_VAR([ASCIIDOC], Asciidoc executable)
	AC_PATH_PROG([ASCIIDOC], [asciidoc], [not_found])
	if test "x$ASCIIDOC" == "xnot_found"; then
		AC_MSG_ERROR([Asciidoc could not be located in your \$PATH])
	fi
	ver_info=`$ASCIIDOC --version`
	ver_maj=`echo $ver_info | sed 's/^.* \([[0-9]]\)*\.\([[0-9]]\)*\.\([[0-9]]*\).*$/\1/'`
	ver_min=`echo $ver_info | sed 's/^.* \([[0-9]]\)*\.\([[0-9]]\)*\.\([[0-9]]*\).*$/\2/'`
	ver_rev=`echo $ver_info | sed 's/^.* \([[0-9]]\)*\.\([[0-9]]\)*\.\([[0-9]]*\).*$/\3/'`
	prog_version_ok="yes"
	if test $ver_maj -lt 8; then
		prog_version_ok="no"
	fi
	if test $ver_min -lt 4; then
		prog_version_ok="no"
	fi
	if test $ver_rev -lt 0; then
		prog_version_ok="no"
	fi
	if test "$prog_version_ok" !=  "yes"; then
		AC_MSG_ERROR([Asciidoc >= 8.4 is needed. Please upgrade your installation])
	fi
	
	dnl Check for xmlto 
	AC_ARG_VAR([XMLTO], Asciidoc executable)
	AC_PATH_PROG([XMLTO], [xmlto], [not_found])
	if test "x$XMLTO" == "xnot_found"; then
		AC_MSG_ERROR([xmlto could not be located in your \$PATH])
	fi
	ver_info=`$XMLTO --version`
	ver_maj=`echo $ver_info | sed 's/^.* \([[0-9]]\)*\.\([[0-9]]\)*\.\([[0-9]]*\).*$/\1/'`
	ver_min=`echo $ver_info | sed 's/^.* \([[0-9]]\)*\.\([[0-9]]\)*\.\([[0-9]]*\).*$/\2/'`
	ver_rev=`echo $ver_info | sed 's/^.* \([[0-9]]\)*\.\([[0-9]]\)*\.\([[0-9]]*\).*$/\3/'`
	prog_version_ok="yes"
	if test $ver_maj -lt 0; then
		prog_version_ok="no"
	fi
	if test $ver_min -lt 0; then
		prog_version_ok="no"
	fi
	if test $ver_rev -lt 18; then
		prog_version_ok="no"
	fi
	if test "$prog_version_ok" !=  "yes"; then
		AC_MSG_ERROR([xmlto >= 0.0.18 is needed. Please upgrade your installation])
	fi
])
