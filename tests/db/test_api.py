#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil, subprocess

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


# Writes a log message to the log file
def log(id, msg):
	logfile = test.log(id)
	f = open(logfile, "a+")
	print >>f, msg
	f.close()


# Sets up a repository, utilizing the repository cache
def setup_repos(id, setup_fn):
	log(id, "\n*** setup_repos ("+str(id)+")\n")

	repo = test.repo(id)
	wc = test.wc(id)
	cache.load_repos(id, test.name(id), repo, wc, setup_fn, test.log(id))


# Dumps the repository using svnadmin and returns the dumpfile path
def dump_original(id, repos = None):
	log(id, "\n*** dump_original ("+str(id)+")\n")

	if not repos:
		repos = test.repo(id)
	dump = test.dumps(id)+"/original.dump"
	run("svnadmin", "dump", repos, output = dump, error = test.log(id))
	return dump


# Dumps the repository using rsvndump and returns the dumpfile path
def dump_rsvndump(id, args, repos = None):
	log(id, "\n*** dump_rsvndump ("+str(id)+")\n")

	if not repos:
		repos = test.repo(id)
	dump = test.dumps(id)+"/rsvndump.dump"
	run("../../src/rsvndump", "file://"+repos, extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Dumps the subdirectory using rsvndump and returns the dumpfile path
def dump_rsvndump_sub(id, path, args, repos = None):
	log(id, "\n*** dump_rsvndump_sub ("+str(id)+")\n")

	if not repos:
		repos = test.repo(id)
	dump = test.dumps(id)+"/rsvndump.dump"
	run("../../src/rsvndump", "file://"+repos+"/"+path, extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Dumps the reopsitory incremental using rsvndump and returns the dumpfile path
def dump_rsvndump_incremental(id, stepsize, args, repos = None):
	log(id, "\n*** dump_rsvndump_incremental ("+str(id)+")\n")

	if not repos:
		repos = test.repo(id)
	dump = test.dumps(id)+"/rsvndump.dump"
	start = 0
	end = stepsize
	while True:
		try:
			run("../../src/rsvndump", "file://"+repos, "--incremental", "--revision", str(start)+":"+str(end), extra_args = tuple(args), output = dump, error = test.log(id))
			start = end+1
			end = start+stepsize
		except:
			break
	return dump


# Loads the specified dumpfile into a temporary repository and dumps it
def dump_reload(id, dumpfile):
	log(id, "\n*** dump_reload ("+str(id)+")\n")

	tmp = test.mkdtemp(id)
	run("svnadmin", "create", tmp, output = test.log(id))
	run("svnadmin", "load", tmp, input = dumpfile, output = test.log(id))

	dump = test.dumps(id)+"/validate.dump"
	run("svnadmin", "dump", tmp, output = dump, error = test.log(id))
	return dump


# Loads the specified dumpfile into a temporary repository and dumps it using
# rsvndump
def dump_reload_rsvndump(id, dumpfile, args):
	log(id, "\n*** dump_reload ("+str(id)+")\n")

	tmp = test.mkdtemp(id)
	run("svnadmin", "create", tmp, output = test.log(id))
	run("svnadmin", "load", tmp, input = dumpfile, output = test.log(id))

	dump = test.dumps(id)+"/validate.dump"
	run("../../src/rsvndump", "file://"+tmp, extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Loads the specified dumpfile into a temporary repository and dumps a
# given subdirectory of it using rsvndump 
def dump_reload_rsvndump_sub(id, dumpfile, path, args):
	log(id, "\n*** dump_reload ("+str(id)+")\n")

	tmp = test.mkdtemp(id)
	run("svnadmin", "create", tmp, output = test.log(id))
	run("svnadmin", "load", tmp, input = dumpfile, output = test.log(id))

	dump = test.dumps(id)+"/validate.dump"
	run("../../src/rsvndump", "file://"+tmp+"/"+path, extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Creates a temporary file and returns a reference to it
def mktemp(id):
	return test.mktemp(id)


# Diffs two files, returning True if they are equal
def diff(id, file1, file2):
	log(id, "\n*** diff ("+file1+", "+file2+")\n")

	diff = test.log(id)+".diff"
	try:
		run("diff", "-Naur", file1, file2, output = diff)
	except:
		return False
	return True


# Applies a given patch to a given file
def patch(id, file, patch):
	log(id, "\n*** patch ("+file+", "+patch+")\n")

	try:
		run("patch", file, patch, output = test.log(id))
	except:
		return False
	return True


# Applies a given binary patch to a given file
def bspatch(id, file, patch):
	log(id, "\n*** bspatch ("+file+", "+patch+")\n")

	tmp = mktemp(id)
	try:
		run("bspatch", file, tmp, patch, output = test.log(id), error = test.log(id))
		shutil.move(tmp, file)
	except:
		return False
	return True
