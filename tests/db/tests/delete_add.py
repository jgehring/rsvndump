#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os

import test_api


def info():
	return "Add after delete test"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		f = open("dir1/file1","w")
		print >>f, "hello1"
		print >>f, "hello2"
		f = open("dir1/file2","w")
		print >>f, "hello3"
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		f = open("file1","w")
		print >>f, "hello4"
		f = open("file12","w")
		print >>f, "hello5"
		test_api.run("svn", "add", "file1", "file12", output = log)
		return True
	elif step == 2:
		test_api.run("svn", "rm", "file1", output=log)
		return True
	elif step == 3:
		f = open("file12","a")
		print >>f, "hello6"
		return True
	elif step == 4:
		test_api.run("svn", "rm", "dir1", output=log)
		return True
	elif step == 5:
		os.mkdir("dir1")
		f = open("dir1/file1","w")
		print >>f, "hello7"
		f = open("dir1/file2","w")
		print >>f, "hello8"
		print >>f, "hello9"
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 6:
		f = open("dir1/file1","a")
		print >>f, "hello10"
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

