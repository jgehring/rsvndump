#!/bin/sh
#
#	A small script to invoke autotools.
#
#	Please note that this file is (of course) NOT licensed under the GPL,
#	as this would make absolutely no sense for 4 lines of sh code.
#	Therfore, it is released into the public domain.
#

exec autoreconf --warnings=all --install --verbose "$@"
