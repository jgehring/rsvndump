#!/usr/bin/env python
#
#	rsvndump test suite
#


import os

import test_api


def info():
	return "Simple deletion test"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		os.mkdir("dir1/sdir1")
		os.mkdir("dir1/sdir2")
		f = open("dir1/file1","wb")
		f.write(b"hello1\n")
		f = open("dir1/sdir1/file1","wb")
		f.write(b"hello2\n")
		f = open("dir1/sdir2/file1","wb")
		f.write(b"hello3\n")
		f = open("dir1/sdir2/file2","wb")
		f.write(b"hello4\n")
		test_api.run("svn", "add", "dir1", output=log)
		return 1
	elif step == 1:
		f = open("dir1/sdir2/file2","ab")
		f.write(b"hello5\n")
		return 1
	elif step == 2:
		f = open("dir1/sdir1/file2","wb")
		f.write(b"hello6\n")
		test_api.run("svn", "add", "dir1/sdir1/file2", output=log)
		return 1
	elif step == 3:
		test_api.run("svn", "up", output=log)
		test_api.run("svn", "rm", "dir1/sdir1", output=log)
		return 1
	elif step == 4:
		test_api.run("svn", "rm", "dir1/sdir2/file1", output=log)
		return 1
	elif step == 5:
		f = open("dir1/sdir2/file2","ab")
		f.write(b"still there!\n")
		return 1
	else:
		return 0

# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	odump_path = test_api.dump_original(id)
	rdump_path = test_api.dump_rsvndump(id, args)
	vdump_path = test_api.dump_reload(id, rdump_path)

	return test_api.diff(id, odump_path, vdump_path)
