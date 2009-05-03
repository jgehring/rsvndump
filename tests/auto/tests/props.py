#!/usr/bin/env python
#
#	rsvndump test suite
#


import os

from run import run


def info():
	return "Property test"


def modify_tree(step, logfile):
	if step == 0:
		os.mkdir("dir1")
		f = open("dir1/file1",'w')
		print >>f, 'hello1'
		run("svn", "add", "dir1", output=logfile)
		return 1
	elif step == 1:
		run("svn", "propset", "copyright", "(c) ME", "dir1/file1", output=logfile)
		run("svn", "propset", "license", "public domain", "dir1/file1", output=logfile)
		run("svn", "propset", "bla", "blubb", "dir1/file1", output=logfile)
		return 1
	elif step == 2:
		run("svn", "up", output=logfile)
		run("svn", "propset", "svn:ignore", "*.o", "dir1", output=logfile)
		return 1
	elif step == 3:
		os.mkdir("dir2")
		run("svn", "add", "dir2", output=logfile)
		run("svn", "propset", "svn:externals", "ext/test    svn://slug/rsvndump/trunk/src", "dir2", output=logfile)
		return 1
	else:
		return 0

