#!/usr/bin/env python
#
#	rsvndump test suite
#


import os


def modify_tree(path, step):
	if step < 5:
		print("Commit step "+str(step))
		return 1
	else:
		return 0
