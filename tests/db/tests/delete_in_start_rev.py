#!/usr/bin/env python
#
#	rsvndump test suite
#
#   test fix for https://github.com/jgehring/rsvndump/issues/18
#


import os

import test_api


def info():
	return "path deleted in first dumped revision should have no nodes in dump file"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		with open("dir1/file1", "wb") as f:
			f.write(b"hello1\n")
		with open("file2", "wb") as f:
			f.write(b"hello2\n")
		test_api.run("svn", "add", "dir1", "file2", output=log)
		return True
	elif step == 1:
		test_api.run("svn", "rm", "dir1/file1", output=log)
		return True
	else:
		return False

# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	# arguments to dump only the second revision
	rev_range_args = ["-r", "2:2"]
	args += rev_range_args

	# generate dumps with svnadmin load and rsvndump
	odump_path = test_api.dump_original(id, rev_range_args)
	rdump_path = test_api.dump_rsvndump(id, args)

	# check dumps can be loaded by svnadmin load
	repo1 = test_api.repos_load(id, odump_path)
	repo2 = test_api.repos_load(id, rdump_path)
	if "--keep-revnums" not in args:
		# checks both repositories have same content
		return test_api.diff_repos(id, repo1, "", repo2, "")
	else:
		# when --keep-revnums option is used we cannot compare the two
		# repos as the rsvndump one has an extra revision, nevertheless
		# its correct loading by svnadmin has been previously tested
		return True
