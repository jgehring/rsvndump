#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, subprocess

import test, cache


# Runs a external program
def run(*args, **misc):
	redirections = {}
	extra_args = misc.pop("extra_args", None)
	input = misc.pop("input", None)
	output = misc.pop("output", None)
	error = misc.pop("error", None)
	if extra_args:
		args += extra_args
	if input:
		redirections['stdin'] = open(input, "r")
	if output:
		redirections['stdout'] = open(output, "a+")
	if error:
		redirections['stderr'] = open(error, "a+")
	subprocess.check_call(args, **redirections)


# Returns the tests data dir
def data_dir():
	return test.cwd+"/"+test.tests_dir+"/data"

# Returns the name of a test
def name(id):
	return test.name(id)

# Setups a repository, utilizing the repository cache
def setup_repos(id, setup_fn):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** setup_repos ("+str(id)+")\n"

	repo = test.repo(id)
	wc = test.wc(id)
	cache.load_repos(id, test.name(id), repo, wc, setup_fn, logfile)


# Dumps the repository using svnadmin and returns the dumpfile path
def dump_original(id):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** dump_original ("+str(id)+")\n"

	dump = test.dumps(id)+"/original.dump"
	run("svnadmin", "dump", test.repo(id), output = dump, error = logfile)
	return dump


# Dumps the repository using rsvndump and returns the dumpfile path
def dump_rsvndump(id, args):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** dump_rsvndump ("+str(id)+")\n"

	dump = test.dumps(id)+"/rsvndump.dump"
	run("../../src/rsvndump", "file://"+test.repo(id), extra_args = tuple(args), output = dump, error = logfile)
	return dump


# Dumps the subdirectory using rsvndump and returns the dumpfile path
def dump_rsvndump_sub(id, path, args):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** dump_rsvndump_sub ("+str(id)+")\n"

	dump = test.dumps(id)+"/rsvndump.dump"
	run("../../src/rsvndump", "file://"+test.repo(id)+"/"+path, extra_args = tuple(args), output = dump, error = logfile)
	return dump


# Dumps the reopsitory incremental using rsvndump and returns the dumpfile path
def dump_rsvndump_incremental(id, stepsize, args):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** dump_rsvndump_incremental ("+str(id)+")\n"

	dump = test.dumps(id)+"/rsvndump.dump"
	start = 0
	end = stepsize
	while True:
		try:
			run("../../src/rsvndump", "file://"+test.repo(id), "--incremental", "--revision", str(start)+":"+str(end), extra_args = tuple(args), output = dump, error = logfile)
			start = end+1
			end = start+stepsize
		except:
			break
	return dump


# Loads the specified dumpfile into a temporary repository and dumps it
def dump_reload(id, dumpfile):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** dump_reload ("+str(id)+")\n"

	tmp = test.mkdtemp(id)
	run("svnadmin", "create", tmp, output = logfile)
	run("svnadmin", "load", tmp, input = dumpfile, output = logfile)

	dump = test.dumps(id)+"/validate.dump"
	run("svnadmin", "dump", tmp, output = dump, error = logfile)
	return dump


# Creates a temporary file and returns a reference to it
def mktemp(id):
	return test.mktemp(id)


# Diffs two files, returning True if they are equal
def diff(id, file1, file2):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, "\n*** diff ("+file1+", "+file2+")\n"

	diff = test.log(id)+".diff"
	try:
		run("diff", "-Naur", file1, file2, output = diff)
	except:
		return False
	return True
