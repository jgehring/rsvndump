#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Directory copy from padded revision"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		os.mkdir("dir2")
		os.mkdir("dir1/sdir1")
		f = open("dir1/sdir1/file1",'wb')
		f.write(b"hello1\n")
		os.mkdir("dir2/sdir2")
		f = open("dir2/sdir2/file2",'wb')
		f.write(b"hello2\n")
		test_api.run("svn", "add", "dir1", output = log)
		test_api.run("svn", "add", "dir2", output = log)
		return True
	elif step == 1:
		f = open("dir1/sdir1/file1",'ab')
		f.write(b"hello3\n")
		return True
	elif step == 2:
		f = open("dir2/sdir2/file2",'ab')
		f.write(b"hello4\n")
		return True
	elif step == 3:
		f = open("dir2/sdir2/file3",'wb')
		f.write(b"hello5\n")
		test_api.run("svn", "add", "dir2/sdir2/file3", output = log)
		return True
	elif step == 4:
		test_api.run("svn", "up", output = log);
		test_api.run("svn", "cp", "dir1/sdir1", "dir1/dir2", output = log)
		return True
	elif step == 5:
		f = open("dir2/file1",'ab')
		f.write(b"just copied!\n")
		return True
	else:
		return False

# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	args.append("--keep-revnums")
	odump_path = test_api.dump_original(id)
	rdump_path = test_api.dump_rsvndump_sub(id, "dir1", args)
	try:
		test_api.dump_reload(id, rdump_path)
	except:
		return False
	return True
