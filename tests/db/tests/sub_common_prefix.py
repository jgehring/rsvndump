#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Subdirectories with common prefix (#3544240)"


def setup(step, log):
	if step == 0:
		os.mkdir("foo")
		os.mkdir("foo/sdir1")
		os.mkdir("foo/sdir2")
		os.mkdir("foobar")
		os.mkdir("foobar/sdir1")

		f = open("foo/file1","wb")
		print >>f, "hello1"
		f = open("foo/sdir1/file1","wb")
		print >>f, "hello2"
		f = open("foo/sdir2/file1","wb")
		print >>f, "hello3"
		f = open("foobar/file1","wb")
		print >>f, "hello bar"
		f = open("foobar/sdir1/file1","wb")
		print >>f, "hello bar sub"
		test_api.run("svn", "add", "foo", output = log)
		test_api.run("svn", "add", "foobar", output = log)
		return True
	elif step == 1:
		f = open("foobar/file2","wb")
		print >>f, "hello4"
		test_api.run("svn", "add", "foobar/file2", output = log)
		return True
	elif step == 2:
		test_api.run("svn", "rm", "foobar/sdir1", output = log)
		f = open("foo/file2","wb")
		print >>f, "hello5"
		test_api.run("svn", "add", "foo/file2", output = log)
		return True
	elif step == 3:
		f = open("foo/file3","wb")
		print >>f, "hello5"
		test_api.run("svn", "add", "foo/file3", output = log)
		return True
	elif step == 4:
		test_api.run("svn", "rm", "foo/file2", output = log)
		return True
	else:
		return False


# Runs the test
def run(id, args = []):
	# Set up the test repository
	repo1 = test_api.setup_repos(id, setup)

	args.append("--prefix")
	args.append("foo/");
	rdump_path = test_api.dump_rsvndump_sub(id, "foo", args)

	repo2 = test_api.repos_load(id, rdump_path)
	return test_api.diff_repos(id, repo1, "foo", repo2, "foo")
