#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os

import test_api


def info():
	return "Simple test"


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
		f = open("dir1/file2","wb")
		f.write(b"hello4\n")
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
