#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil, sys, tempfile


# Globals
tests_dir = "tests"
repo_dir = "repos"
log_dir = "logs"
dump_dir = "dumps"
tmp_dir = "tmp"
wc_dir = "wcs"
dirs = [wc_dir, repo_dir, log_dir, dump_dir, tmp_dir]
cwd = str(os.getcwd())


def load(test):
	sys.path.insert(0, os.getcwd())
	sys.path.insert(0, os.getcwd()+"/"+tests_dir)
	module = None
	try:
		module = __import__(test, None, None, [''])
	except:
		del sys.path[0]
		del sys.path[0]
		raise
	del sys.path[0]
	del sys.path[0]
	return module


# Checks if a module is a valid test
def check_test(test):
	try:
		module = load(test)
		if not hasattr(module, "info"):
			return False
		if not hasattr(module, "run"):
			return False
	except:
		raise
		return False
	return True


# Enumerate all tests available
def all_tests():
	flist = os.listdir(tests_dir)
	for i in flist:
		if i.endswith(".py"):
			name = i[:-3]	# Strip .py extension
			if check_test(name):
				yield name


# Returns the test's info string 
def info(test):
	module = load(test)
	return module.info()


# Cleans up the working directories
def cleanup():
	global dirs
	for d in dirs:
		shutil.rmtree(d)
	setup_dirs()


# Sets up all working directories
def setup_dirs():
	global dirs
	for d in dirs:
		try:
			os.stat(d)
		except:
			try:
				os.mkdir(d)
			except:
				return False
	return True


# Returns a valid ID for a test
def id(test):
	if not setup_dirs():
		return None
	i = 1
	while True:
		tid = test+"_"+str(i)
		try:
			os.mkdir(wc_dir+"/"+tid)
			return tid
		except:
			i += 1


# Returns the name of a test, given its ID
def name(tid):
	return tid[0:tid.rfind("_")]

# Returns the path of a test module, given its ID
def path(tid):
	return cwd+"/"+tests_dir+"/"+name(tid)+".py"

# Returns the logfile for the given test ID
def log(tid):
	return cwd+"/"+log_dir+"/"+tid

# Returns the repository dir for the given test ID
def repo(tid):
	return cwd+"/"+repo_dir+"/"+tid

# Returns the dump file dir for the given test ID
def dumps(tid):
	return cwd+"/"+dump_dir+"/"+tid

# Returns the working copy dir for the given test ID
def wc(tid):
	return cwd+"/"+wc_dir+"/"+tid

# Returns a temporary directory for a given test ID
def mkdtemp(tid):
	return tempfile.mkdtemp("", cwd+"/"+tmp_dir+"/"+tid)


# Runs a test
def run(test, tid, args):
	module = None
	try:
		module = load(test)
	except:
		print("ERROR: No such test: "+test)
		raise

	# Setup working environment
	os.mkdir(repo_dir+"/"+tid)
	os.mkdir(dump_dir+"/"+tid)
	os.mkdir(tmp_dir+"/"+tid)

	ret = False
	try:
		ret = module.run(tid, args)
	except:
		raise
	return ret
