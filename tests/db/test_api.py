#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, platform, re, shutil, subprocess, sys

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

# Runs a external program
def run_noa(*args, **misc):
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
		redirections['stdout'] = open(output, "w+")
	if error:
		redirections['stderr'] = open(error, "a+")
	subprocess.check_call(args, **redirections)


# Returns a valid file URI for both sane operating systems and windows
def uri(loc):
	if not platform.system() == "Windows":
		return loc
	t = loc.replace("://", ":///")
	t = t.replace(os.sep, "/")
	return t


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
	return repo


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
	if not platform.system() == "Windows":
		run("../../src/rsvndump", uri("file://"+repos), extra_args = tuple(args), output = dump, error = test.log(id))
	else:
		run("../../bin/rsvndump.exe", uri("file://"+repos), extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Dumps the subdirectory using rsvndump and returns the dumpfile path
def dump_rsvndump_sub(id, path, args, repos = None):
	log(id, "\n*** dump_rsvndump_sub ("+str(id)+")\n")

	if not repos:
		repos = test.repo(id)
	dump = test.dumps(id)+"/rsvndump.dump"
	if not platform.system() == "Windows":
		run("../../src/rsvndump", uri("file://"+repos+"/"+path), extra_args = tuple(args), output = dump, error = test.log(id))
	else:
		run("../../bin/rsvndump.exe", uri("file://"+repos+"/"+path), extra_args = tuple(args), output = dump, error = test.log(id))
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
			if not platform.system() == "Windows":
				run("../../src/rsvndump", uri("file://"+repos), "--incremental", "--no-incremental-header", "--revision", str(start)+":"+str(end), extra_args = tuple(args), output = dump, error = test.log(id))
			else:
				run("../../bin/rsvndump.exe", uri("file://"+repos), "--incremental", "--no-incremental-header", "--revision", str(start)+":"+str(end), extra_args = tuple(args), output = dump, error = test.log(id))
			start = end+1
			end = start+stepsize
		except:
			break
	return dump

# Dumps the reopsitory incremental using rsvndump and returns the dumpfile path
def dump_rsvndump_incremental_sub(id, path, stepsize, args, repos = None):
	log(id, "\n*** dump_rsvndump_incremental ("+str(id)+")\n")

	if not repos:
		repos = test.repo(id)
	dump = test.dumps(id)+"/rsvndump.dump"

	# Fetch history
	history = mktemp(id)
	run("svnlook", "history", repos, path, output = history, error = test.log(id))
	f = open(history, "r")
	hist = f.readlines()
	f.close()
	hist.reverse()

	# Filter history
	regex = re.compile(" *[0-9]*   \/"+path+"$", re.IGNORECASE)
	hist = [h for h in hist if regex.search(h)]

	# Iteratve over history
	regex = re.compile(" *([0-9]*)", re.IGNORECASE)
	start = 0
	end = 0
	for h in hist:
		match = regex.match(h)
		if not match:
			break
		end = int(match.group())
		if end-start < stepsize:
			continue

		if not platform.system() == "Windows":
			run("../../src/rsvndump", uri("file://"+repos+"/"+path), "--incremental", "--no-incremental-header", "--revision", str(start)+":"+str(end), extra_args = tuple(args), output = dump, error = test.log(id))
		else:
			run("../../bin/rsvndump", uri("file://"+repos+"/"+path), "--incremental", "--no-incremental-header", "--revision", str(start)+":"+str(end), extra_args = tuple(args), output = dump, error = test.log(id))
		start = end+1

	return dump


# Loads the specified dumpfile into a temporary repository and returns a path to it
def repos_load(id, dumpfile):
	log(id, "\n*** repos_load ("+str(id)+")\n")

	tmp = test.mkdtemp(id)
	run("svnadmin", "create", tmp, output = test.log(id))
	run("svnadmin", "load", tmp, input = dumpfile, output = test.log(id))

	return tmp 


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
	if not platform.system() == "Windows":
		run("../../src/rsvndump", uri("file://"+tmp), extra_args = tuple(args), output = dump, error = test.log(id))
	else:
		run("../../bin/rsvndump.exe", uri("file://"+tmp), extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Loads the specified dumpfile into a temporary repository and dumps a
# given subdirectory of it using rsvndump 
def dump_reload_rsvndump_sub(id, dumpfile, path, args):
	log(id, "\n*** dump_reload ("+str(id)+")\n")

	tmp = test.mkdtemp(id)
	run("svnadmin", "create", tmp, output = test.log(id))
	run("svnadmin", "load", tmp, input = dumpfile, output = test.log(id))

	dump = test.dumps(id)+"/validate.dump"
	if not platform.system() == "Windows":
		run("../../src/rsvndump", uri("file://"+tmp+"/"+path), extra_args = tuple(args), output = dump, error = test.log(id))
	else:
		run("../../bin/rsvndump.exe", uri("file://"+tmp+"/"+path), extra_args = tuple(args), output = dump, error = test.log(id))
	return dump


# Compares two subversion repositories using "svnlook"
def diff_repos(id, repo1, sub1, repo2, sub2):
	log(id, "\n*** compare_repos ("+str(id)+"): "+repo1+"/"+sub1+" and "+repo2+"/"+sub2+"\n")

	# Retrieve log messages
	log1 = test.mktemp(id)
	log2 = test.mktemp(id)
	run("svnlook", "history", repo1, sub1, output = log1, error = test.log(id))
	run("svnlook", "history", repo2, sub2, output = log2, error = test.log(id))

	f1 = open(log1, "r")
	rev1 = f1.readlines()
	rev1.reverse()
	f2 = open(log2, "r")
	rev2 = f2.readlines()
	f2.close()
	f1.close()
	rev2.reverse()

	# Filter logs
	regex = re.compile(" *[0-9]*   \/"+sub1+"$", re.IGNORECASE)
	rev1 = [rev for rev in rev1 if regex.search(rev)]
	regex = re.compile(" *[0-9]*   \/"+sub2+"$", re.IGNORECASE)
	rev2 = [rev for rev in rev2 if regex.search(rev)]

	if len(rev1) != len(rev2):
		log(id, "\n"+str(rev1))
		log(id, "\n"+str(rev2))
		return False

	# Compare trees & file contents
	out1 = mktemp(id)
	out2 = mktemp(id)
	fout1 = mktemp(id)
	fout2 = mktemp(id)
	diff = test.log(id)+".diff"
	rx1 = re.compile(" *([0-9]*)", re.IGNORECASE)
	for rev in zip(rev1, rev2):
		r1 = int(rx1.match(rev[0]).group())
		r2 = int(rx1.match(rev[1]).group())
		run_noa("svnlook", "tree", "--full-paths", "-r", str(r1), repo1, sub1, output = out1, error = test.log(id))
		run_noa("svnlook", "tree", "--full-paths", "-r", str(r2), repo2, sub2, output = out2, error = test.log(id))
		log(id, "comparing trees at revision "+str(r1)+" and "+str(r2))
		try:
			run("diff", "-Naur", out1, out2, output = diff)
		except:
			return False

		# Compare files
		f = open(out1, "r")
		for line in f.readlines():
			file = line.lstrip()[:-1]
			if file.endswith("/"): # Skip directories
				continue

			run_noa("svnlook", "cat", "-r", str(r1), repo1, file, output = fout1, error = test.log(id))
			run_noa("svnlook", "cat", "-r", str(r2), repo2, file, output = fout2, error = test.log(id))
			try:
				run("diff", "-Naur", fout1, fout2, output = diff)
			except:
				log(id, "  failed, file "+file+" differs!")
				return False

		f.close()

	return True


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
	except OSError:
		sys.stderr.write("'bspatch' executable missing?\n")
		raise
	except:
		return False
	return True
