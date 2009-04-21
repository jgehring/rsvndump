#!/usr/bin/env python
#
#	rsvndump test suite
#


import os

from run import run


def info():
	return "Sample test"


def modify_tree(step, logfile):
	if step < 5:
		# Do nothing
		return 1
	else:
		return 0
