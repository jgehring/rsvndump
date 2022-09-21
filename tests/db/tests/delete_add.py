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
		f = open("dir1/file1","wb")
		f.write(b"hello1\n")
		f.write(b"hello2\n")
		f = open("dir1/file2","wb")
		f.write(b"hello3\n")
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		f = open("file1","wb")
		f.write(b"hello4\n")
		f = open("file12","wb")
		f.write(b"hello5\n")
		test_api.run("svn", "add", "file1", "file12", output = log)
		return True
	elif step == 2:
		test_api.run("svn", "rm", "file1", output=log)
		return True
	elif step == 3:
		f = open("file12","ab")
		f.write(b"hello6\n")
		return True
	elif step == 4:
		test_api.run("svn", "rm", "dir1", output=log)
		return True
	elif step == 5:
		os.mkdir("dir1")
		f = open("dir1/file1","wb")
		f.write(b"hello7\n")
		f = open("dir1/file2","wb")
		f.write(b"hello8\n")
		f.write(b"hello9\n")
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 6:
		f = open("dir1/file1","ab")
		f.write(b"hello10\n")
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

