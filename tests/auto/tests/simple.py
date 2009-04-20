#!/usr/bin/env python
#
#	rsvndump test suite
#


import os

from run import run


def info():
	return "Simple test"


def modify_tree(step, logfile):
	if step == 0:
		os.mkdir("dir1")
		f = open("dir1/file1",'w')
		print >>f, 'hello1'
		print >>f, 'hello2'
		f = open("dir1/file2",'w')
		print >>f, 'hello3'
		run("svn", "add", "dir1", output=logfile)
		return 1
	elif step == 1:
		f = open("dir1/file2",'w')
		print >>f, 'hello4'
		return 1
	else:
		return 0
