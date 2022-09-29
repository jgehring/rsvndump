#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil

import test_api


def info():
	return "Prefix test"


def setup(step, log):
	if step == 0:
		os.mkdir("dir1")
		f = open("dir1/file1","wb")
		f.write(b"hello1\n")
		f.write(b"hello2\n")
		f = open("dir1/file2","wb")
		f.write(b"hello3\n")
		test_api.run("svn", "add", "dir1", output = log)
		return True
	elif step == 1:
		f = open("dir1/file2","wb")
		f.write(b"hello4\n")
		return True
	elif step == 2:
		f = open("dir1/file2","wb")
		f.write(b"hello5\n")
		return True
	else:
		return False


# Runs the test
def run(id, args = []):
	# Set up the test repository
	test_api.setup_repos(id, setup)

	odump_path = test_api.dump_original(id)

	# Dump with prefix
	my_prefix = "a/b/c/d/e/f/";
	args.append("--prefix")
	args.append(my_prefix)
	rdump_path = test_api.dump_rsvndump(id, args)

	# Load & dump subdirectory prefix 
	vdump_path = test_api.dump_reload_rsvndump_sub(id, rdump_path, my_prefix, args[:-2])
	tmp = test_api.mktemp(id)
	shutil.move(vdump_path, tmp)

	# A final verification dump
	vdump_path = test_api.dump_reload(id, tmp)

	# Don't compare the UUID (is different due to prefix and subpath dumping)
	# and the date of the first revision
	files = [vdump_path, odump_path]
	for f in files:
		tmp = test_api.mktemp(id)
		o = open(tmp, "w")
		cnt = -1
		for line in open(f):
			if line.startswith("UUID: "):
				continue
			elif line == "svn:date\n":
				cnt = 2
			elif cnt > 0:
				cnt -= 1
				if cnt == 0:
					continue
			o.write(line)
		o.close()
		shutil.move(tmp, f)

	return test_api.diff(id, odump_path, vdump_path)
