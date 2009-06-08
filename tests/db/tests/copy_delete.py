#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os

import test_api


def info():
	return "Copying test with deletions in the same revision"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		os.mkdir("dir1/sdir1")
		os.mkdir("dir1/sdir2")
		f = open("dir1/file1","wb")
		print >>f, "hello1"
		f = open("dir1/sdir1/file1","wb")
		print >>f, "hello2"
		f = open("dir1/sdir2/file1","wb")
		print >>f, "hello3"
		f = open("dir1/sdir2/file2","wb")
		print >>f, "hello4"
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		f = open("dir1/sdir1/file2","wb")
		print >>f, "hello6"
		test_api.run("svn", "add", "dir1/sdir1/file2", output = log)
		return True
	elif step == 2:
		test_api.run("svn", "cp", "dir1", "dir2", output = log)
		test_api.run("svn", "rm", "dir2/sdir1", output = log)
		test_api.run("svn", "rm", "dir2/sdir2/file1", output = log)
		return True
	elif step == 3:
		f = open("dir2/sdir2/file2","ab")
		print >>f, "just copied!"
		return True
	else:
		return False


# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	odump_path = test_api.dump_original(id)
	rdump_path = test_api.dump_rsvndump(id, args)
	vdump_path = test_api.dump_reload(id, rdump_path)

	return test_api.diff(id, odump_path, vdump_path)
