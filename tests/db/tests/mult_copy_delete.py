#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os

import test_api


def info():
	return "Multiple copies with deletions (#2898487)"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		f = open("dir1/file1","wb")
		print >>f, "content_of_file1"
		f = open("dir1/file2","wb")
		print >>f, "content_of_file2"
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		os.mkdir("dir1/subdir")
		f = open("dir1/subdir/file5","wb")
		print >>f, "content_of_file5"
		f = open("dir1/subdir/file6","wb")
		print >>f, "content_of_file6"
		test_api.run("svn", "add", "dir1/subdir", output = log)
		return True
	elif step == 2:
		f = open("dir1/subdir/file7","wb")
		print >>f, "content_of_file7"
		f = open("dir1/subdir/file8","wb")
		print >>f, "content_of_file8"
		test_api.run("svn", "add", "dir1/subdir/file7", "dir1/subdir/file8", output = log)
		return True
	elif step == 3:
		test_api.run("svn", "up", output = log)
		test_api.run("svn", "cp", "dir1@2", "dir2", output = log)
		test_api.run("svn", "rm", "dir2/file1", output = log)
		test_api.run("svn", "rm", "dir2/file2", output = log)
		f = open("dir2/file3","wb")
		print >>f, "content_of_file3"
		f = open("dir2/file4","wb")
		print >>f, "content_of_file4"
		test_api.run("svn", "add", "dir2/file3", "dir2/file4", output = log)
		test_api.run("svn", "rm", "dir2/subdir/file5", output = log)
		test_api.run("svn", "rm", "dir2/subdir/file6", output = log)
		test_api.run("svn", "rm", "dir2/subdir", output = log)
		test_api.run("rm", "-rf", "dir2/subdir", output = log)
		test_api.run("svn", "cp", "dir1/subdir", "dir2", output = log)

		f = open("dir2/subdir/file5_new","wb")
		print >>f, "content_of_file5_new"
		f = open("dir2/subdir/file6_new","wb")
		print >>f, "content_of_file6_new"
		test_api.run("svn", "add", "dir2/subdir/file5_new", "dir2/subdir/file6_new", output = log)
		test_api.run("svn", "rm", "dir2/subdir/file5", "dir2/subdir/file6", output = log)

		f = open("dir2/subdir/file7_new","wb")
		print >>f, "content_of_file7_new"
		f = open("dir2/subdir/file8_new","wb")
		print >>f, "content_of_file8_new"
		test_api.run("svn", "add", "dir2/subdir/file7_new", "dir2/subdir/file8_new", output = log)
		test_api.run("svn", "rm", "dir2/subdir/file7", "dir2/subdir/file8", output = log)

		test_api.run("svn", "rm", "dir1/", output = log)
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
