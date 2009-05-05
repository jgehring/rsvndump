#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Subdirectory copying test"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		os.mkdir("dir1/sdir1")
		os.mkdir("dir1/sdir2")
		f = open("dir1/file1",'w')
		print >>f, 'hello1'
		f = open("dir1/sdir1/file1",'w')
		print >>f, 'hello2'
		f = open("dir1/sdir2/file1",'w')
		print >>f, 'hello3'
		f = open("dir1/sdir2/file2",'w')
		print >>f, 'hello4'
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		f = open("dir1/sdir2/file2",'a')
		print >>f, 'hello5'
		return True
	elif step == 2:
		f = open("dir1/sdir1/file2",'w')
		print >>f, 'hello6'
		test_api.run("svn", "add", "dir1/sdir1/file2", output = log)
		return True
	elif step == 3:
		test_api.run("svn", "cp", "dir1/sdir1", "sdir1", output = log)
		return True
	elif step == 4:
		f = open("sdir1/file1",'a')
		print >>f, "just copied!"
		return True
	else:
		return False


# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	odump_path = test_api.dump_original(id)
	rdump_path = test_api.dump_rsvndump_sub(id, "sdir1", args)
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
