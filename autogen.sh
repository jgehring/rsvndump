#!/bin/sh
#
#	A small script to invoke autotools.
#
#	Please note that this file is (of course) NOT licensed under the GPL,
#	as this would make absolutely no sense for 4 lines of sh code.
#	Therfore, it is released into the public domain.
#

autoheader || (echo "autoheader failed" && exit 1)
aclocal -I m4 || (echo "aclocal failed" && exit 1)
automake --add-missing --copy || (echo "automake failed" && exit 1)
autoconf || (echo "autoconf failed" && exit 1)
