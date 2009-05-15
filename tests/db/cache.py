#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, shutil, hashlib

import test
from test_api import run


# Globals
cache_dir = "cache"


# Ensures the cache directory exists
def ensure_dir():
	try:
		os.mkdir(cache_dir)
	except:
		pass


# Clears the cache
def clear():
	ensure_dir()
	shutil.rmtree(cache_dir)
	ensure_dir()


# Checks if a cache entry is dirty because the test module has changed
def dirty(id, name):
	try:
		m = hashlib.sha224()
		f = open(test.path(id), "r")
		m.update(f.read())
		f.close()
		f = open(cache_dir+"/"+name+".sha")
		digest = f.read()
		f.close()
		return (m.hexdigest() != digest)
	except:
		return True


# Creates new cache entry
def insert(id, name, repos, wc, setup_fn, log):
	# Checkout and commit
	tmp = os.getcwd()
	os.chdir(wc+"/..")
	run("svn", "checkout", "file://"+repos, output = log)
	os.chdir(id)
	step = 0
	while setup_fn(step, log):
		os.chdir(wc+"/..")
		run("svn", "commit", id, "-m", "commit step "+str(step), output = log)
		step += 1
		os.chdir(id)
	os.chdir(tmp)

	# Create dump
	run("svnadmin", "dump", repos, output = cache_dir+"/"+name, error = log)

	# Save the module's sha-1 sum
	m = hashlib.sha224()
	f = open(test.path(id), "r")
	m.update(f.read())
	f.close()
	f = open(cache_dir+"/"+name+".sha", "w")
	print >>f, m.hexdigest(),
	f.close()


# Creates a repository in the given directory using
# the given setup function
def load_repos(id, name, repos, wc, setup_fn, log):
	# Create the repository first
	run("svnadmin", "create", repos, output = log)

	ensure_dir()
	try:
		os.stat(cache_dir+"/"+name)
	except:
		# The cache doesn't contain a dump
		# for this repository yet
		insert(id, name, repos, wc, setup_fn, log)	
		return

	# Regenerate if neccessary
	if dirty(id, name):
		os.unlink(cache_dir+"/"+name)
		insert(id, name, repos, wc, setup_fn, log)	
		return
	else:
		# Import the cached dump using svnadmin
		run("svnadmin", "load", repos, input = cache_dir+"/"+name, output = log)
