#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Incremental dump test"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		f = open("dir1/file1", "w")
		print >>f, "hello1"
		print >>f, "hello2"
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		test_api.run("svn", "propset", "eol-style", "LF", "dir1/file1", output = log)
		return True
	elif step == 2:
		f = open("dir1/file2", "w")
		print >>f, "hello3"
		test_api.run("svn", "add", "dir1/file2", output = log)
		return True
	elif step == 3:
		test_api.run("svn", "copy", "dir1", "dir2", output = log)
		return True
	elif step == 4:
		f = open("dir2/file2", "a")
		print >>f, "hello4"
		return True
	elif step == 5:
		os.mkdir("dir2/sdir1")
		f = open("dir2/sdir1/file1", "w")
		print >>f, "hello5"
		f = open("dir1/file1", "w")
		print >>f, "hello6"
		test_api.run("svn", "add", "dir2/sdir1", output = log)
		return True
	elif step == 6:
		f = open("dir2/sdir1/file1", "w")
		print >>f, "hello7"
		f = open("dir2/sdir1/file2", "w")
		print >>f, "hello8"
		test_api.run("svn", "add", "dir2/sdir1/file2", output = log)
		return True
	else:
		return False


# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	args.append("--dump-uuid")
	rdump_path = test_api.dump_rsvndump(id, args)
	vdump_path = test_api.dump_reload(id, rdump_path)

	shutil.move(rdump_path, rdump_path+".orig")
	shutil.move(vdump_path, vdump_path+".orig")

	rdump_path = test_api.dump_rsvndump_incremental(id, 1, args)
	vdump_path = test_api.dump_reload(id, rdump_path)

	return test_api.diff(id, rdump_path+".orig", rdump_path)
