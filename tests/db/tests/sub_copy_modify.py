#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Copying test with modification in the same revision [sub-directory]"


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
		f = open("dir2/sdir2/file1","ab")
		print >>f, "just modified!"
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
	repo1 = test_api.setup_repos(id, setup)

	args.append("--prefix")
	args.append("dir2/");
	rdump_path = test_api.dump_rsvndump_sub(id, "dir2", args)

	# Because of the prefix, the dump needs to be patched (move prefix construction
	# to revision 4, so it can be compared with the original one)
	if "--keep-revnums" in args:
		test_api.patch(id, rdump_path, test_api.data_dir()+"/"+test_api.name(id)+".keep_revnums.patch")

	repo2 = test_api.repos_load(id, rdump_path)
	return test_api.diff_repos(id, repo1, "dir2", repo2, "dir2")
