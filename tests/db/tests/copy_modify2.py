#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os

import test_api

wc_dir = ""


def info():
	return "Copying test with modifications in the same revision [bugreport from Joel Jirak]"


def setup(step, log):
	global wc_dir;
	if step == 0:
		wc_dir = os.path.split(os.getcwd())[1]+"/";
		os.chdir("..");
		f = open(wc_dir+"file1", "wb")
		f.write(b"This is the original text\n")
		test_api.run("svn", "add", wc_dir+"file1", output = log)
		return True
	elif step == 1:
		os.chdir("..")
		test_api.run("svn", "cp", wc_dir+"file1", wc_dir+"file2", output=log)
		return True
	elif step == 2:
		os.chdir("..")
		test_api.run("svn", "cp", wc_dir+"file1", wc_dir+"file3", output=log)
		f = open(wc_dir+"file3", "wb")
		f.write(b"This is the modified text\n")
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
