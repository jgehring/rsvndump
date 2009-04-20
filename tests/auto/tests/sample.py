#!/usr/bin/env python
#
#	rsvndump test suite
#


import os


def info():
	return "Sample test"


def modify_tree(path, step):
	if step < 5:
		# Do nothing
		return 1
	else:
		return 0
