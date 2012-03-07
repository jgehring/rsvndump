#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os

import test_api


def info():
	return "Copying test with modifications on sub-directories [bugreport from Valentin Haenel]"

def setup(step,log):
	if step == 0:
		os.mkdir("a")
		os.mkdir("a/dir1")
		os.mkdir("a/dir2")
		f = open("a/dir1/file1","wb")
		print >>f, "file1"
		f = open("a/dir1/file2","wb")
		print >>f, "file2"
		f = open("a/dir2/file3","wb")
		print >>f, "file3"
		test_api.run("svn", "add", "a", output = log)
		return True
	elif step == 1:
		test_api.run("svn", "cp", "a", "b", output=log)
		test_api.run("svn", "propset", "svn:ignore", "bogus", "b/dir1", output=log)
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
