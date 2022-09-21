#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Copying test with replacement in the same revision [sub-directory]"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		os.mkdir("dir1/sdir1")
		os.mkdir("dir1/sdir2")
		f = open("dir1/file1","wb")
		f.write(b"hello1\n")
		f = open("dir1/sdir1/file1","wb")
		f.write(b"hello2\n")
		f = open("dir1/sdir2/file1","wb")
		f.write(b"hello3\n")
		f = open("dir1/sdir2/file2","wb")
		f.write(b"hello4\n")
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		f = open("dir1/sdir1/file2","wb")
		f.write(b"hello6\n")
		test_api.run("svn", "add", "dir1/sdir1/file2", output = log)
		return True
	elif step == 2:
		os.chdir("dir1/sdir1")
		f = open("file1","ab")
		f.write(b"hello7\n")
		return True
	elif step == 3:
		os.chdir("dir1")
		test_api.run("svn", "up", output = log)
		test_api.run("svn", "cp", "sdir2", "sdir3", output = log)
		return True
	elif step == 4:
		f = open("dir1/sdir3/file2","ab")
		f.write(b"just copied!\n")
		return True
	else:
		return False


# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	odump_path = test_api.dump_original(id)
	rdump_path = test_api.dump_rsvndump_sub(id, "dir1", args)
	tmp = test_api.dump_reload(id, rdump_path)

	vdump_path = test_api.data_dir()+"/"+test_api.name(id)+".dump"
	if "--deltas" in args:
		vdump_path += ".deltas"
	if "--keep-revnums" in args:
		vdump_path += ".keep_revnums"

	# We need to strip the date property from the dumpfile generated
	# by rsvndump in order to validate it properly
	tmp = test_api.mktemp(id)
	o = open(tmp, "w")
	cnt = -1
	for line in open(rdump_path):
		if line == "svn:date\n":
			cnt = 2
		elif cnt > 0:
			cnt -= 1
			if cnt == 0:
				continue
		o.write(line)
	o.close()
	shutil.move(tmp, rdump_path)

	return test_api.diff(id, rdump_path, vdump_path)
