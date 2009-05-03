#!/usr/bin/env python
#
#	rsvndump test suite
#


import os

from run import run


def info():
	return "Copying test with modifcations in the same revision"


def modify_tree(step, logfile):
	if step == 0:
		os.mkdir("dir1")
		os.mkdir("dir1/sdir1")
		os.mkdir("dir1/sdir2")
		f = open("dir1/file1",'w')
		print >>f, 'hello1'
		f = open("dir1/sdir1/file1",'w')
		print >>f, 'hello2'
		f = open("dir1/sdir2/file1",'w')
		print >>f, 'hello3'
		f = open("dir1/sdir2/file2",'w')
		print >>f, 'hello4'
		run("svn", "add", "dir1", output=logfile)
		return 1
	elif step == 1:
		f = open("dir1/sdir1/file2",'w')
		print >>f, 'hello6'
		run("svn", "add", "dir1/sdir1/file2", output=logfile)
		return 1
	elif step == 2:
		run("svn", "cp", "dir1", "dir2", output=logfile)
		f = open("dir2/sdir2/file1",'a')
		print >>f, "just modified!"
		return 1
	elif step == 3:
		f = open("dir2/sdir2/file2",'a')
		print >>f, "just copied!"
		return 1
	else:
		return 0

