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
		os.makedirs("dir/dir2")
		test_api.run("svn", "add", "dir", output = log)
		return True
	elif step == 1:
		os.makedirs("dir/dir1/subdir")
		f = open("dir/dir1/file1", "wb")
		print >>f, "bli"
		f = open("dir/dir1/file2", "wb")
		print >>f, "bla"
		f = open("dir/dir1/subdir/file3", "wb")
		print >>f, "blubb"
		f = open("dir/dir1/subdir/file4", "wb")
		print >>f, "blubb 2"
		f = open("dir/dir1/subdir/file5", "wb")
		print >>f, "blubb"
		f = open("dir/dir1/subdir/file6", "wb")
		print >>f, "blubb 2"
		test_api.run("svn", "add", "dir/dir1", output = log)
		return True
	elif step == 2:
		f = open("dir/dir1/subdir/file6", "wb")
		print >>f, "fixed"
		return True
	elif step == 3:
		f = open("dir/dir2/file7", "wb")
		print >>f, "schibu"
		f = open("dir/dir2/file8", "wb")
		print >>f, "dubi"
		f = open("dir/dir2/file9", "wb")
		print >>f, "du"
		test_api.run("svn", "add", "dir/dir2/file7", "dir/dir2/file8", "dir/dir2/file9", output = log)
		return True
	elif step == 4:
		test_api.run("svn", "mv", "dir/dir1/file1", "dir/dir1/file10", output = log)
		test_api.run("svn", "mv", "dir/dir1/file2", "dir/dir1/file11", output = log)
		return True
	elif step == 5:
		f = open("dir/dir2/Printer.java", "wb")
		print >>f, "juhu"
		test_api.run("svn", "add", "dir/dir2/Printer.java", output = log)
		return True
	elif step == 6:
		f = open("dir/dir2/file7", "wb")
		print >>f, "yippie"
		return True
	elif step == 7:
		f = open("dir/dir2/file7", "wb")
		print >>f, "yeah"
		return True
	elif step == 8:
		test_api.run("svn", "up", output = log)
		test_api.run("svn", "cp", "dir/dir1@2", "dir/dir1_new", output = log)
		test_api.run("svn", "rm", "dir/dir1_new/file1", output = log)
		test_api.run("svn", "rm", "dir/dir1_new/file2", output = log)
		f = open("dir/dir1_new/file12.java", "wb")
		print >>f, "some"
		f = open("dir/dir1_new/file13.java", "wb")
		print >>f, "content"
		test_api.run("svn", "add", "dir/dir1_new/file12.java", "dir/dir1_new/file13.java", output = log)
#		test_api.run("svn", "rm", "dir2/subdir/file5", output = log)
#		test_api.run("svn", "rm", "dir2/subdir/file6", output = log)
#		test_api.run("svn", "rm", "dir2/subdir", output = log)
#		test_api.run("rm", "-rf", "dir2/subdir", output = log)
#		test_api.run("svn", "cp", "dir1/subdir", "dir2", output = log)
#
#		f = open("dir2/subdir/file5_new","wb")
#		print >>f, "content_of_file5_new"
#		f = open("dir2/subdir/file6_new","wb")
#		print >>f, "content_of_file6_new"
#		test_api.run("svn", "add", "dir2/subdir/file5_new", "dir2/subdir/file6_new", output = log)
#		test_api.run("svn", "rm", "dir2/subdir/file5", "dir2/subdir/file6", output = log)
#
#		f = open("dir2/subdir/file7_new","wb")
#		print >>f, "content_of_file7_new"
#		f = open("dir2/subdir/file8_new","wb")
#		print >>f, "content_of_file8_new"
#		test_api.run("svn", "add", "dir2/subdir/file7_new", "dir2/subdir/file8_new", output = log)
#		test_api.run("svn", "rm", "dir2/subdir/file7", "dir2/subdir/file8", output = log)
#		test_api.run("svn", "rm", "dir1/", output = log)
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
