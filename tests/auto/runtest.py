#!/usr/bin/env python
#
#	rsvndump test suite
#


import os
import sys
import subprocess

from run import run


# Some globals
work_dir = "work"
repos_dir = "work/repos"
wc_dir = "work/wc"
dump_dir = "work/dumps"
log_dir = "work/logs"
test_id = ""
extra_args = []


def setup_repos(name, modify):
	# Create all needed directories
	try:
		os.stat(work_dir)
	except:
		os.mkdir(work_dir)
	try:
		os.stat(repos_dir)
	except:
		os.mkdir(repos_dir)
	try:
		os.stat(wc_dir)
	except:
		os.mkdir(wc_dir)
	try:
		os.stat(dump_dir)
	except:
		os.mkdir(dump_dir)
	try:
		os.stat(log_dir)
	except:
		os.mkdir(log_dir)

	# Setup repos direcotry and working copy
	global test_id
	num = 1
	name = os.path.basename(name)
	test_id = name+"_"+str(num)
	current_repos = repos_dir+"/"+test_id
	while 1:
		try:
			os.stat(current_repos)
		except:
			break
		num += 1
		test_id = name+"_"+str(num)
		current_repos = repos_dir+"/"+test_id
	os.mkdir(current_repos)
	print("** ID: '"+test_id+"'")
	print(">> Creating repository...")
	os.system("svnadmin create "+current_repos)
	current_repos = os.getcwd()+"/"+current_repos
	old_dir = os.getcwd()
	os.chdir(wc_dir)
	run("svn", "checkout", "file://"+current_repos, output=old_dir+"/"+log_dir+"/"+test_id)
	os.chdir(test_id)
	
	# Run committer
	print(">> Committing...")
	step = 0
	while modify(step, old_dir+"/"+log_dir+"/"+test_id):
		run("svn", "commit", "-m 'commit step "+str(step)+"'", output=old_dir+"/"+log_dir+"/"+test_id)
#		run("svn", "up", output=old_dir+"/"+log_dir+"/"+test_id)
		step += 1
	os.chdir(old_dir)


def dump_repos(preproc = None):
	print(">> Creating reference dump...")
	dest_dir = dump_dir+"/"+test_id
	try:
		os.stat(dest_dir)
	except:
		os.mkdir(dest_dir)
	run("svnadmin", "dump", repos_dir+"/"+test_id, output=dump_dir+"/"+test_id+"/"+"original.dump", error=log_dir+"/"+test_id)
	if preproc:
		preproc(dump_dir+"/"+test_id+"/"+"original.dump", log_dir+"/"+test_id)


def rsvndump_dump(subdir = None):
	print(">> Running rsvndump...")
	dest_dir = dump_dir+"/"+test_id
	try:
		os.stat(dest_dir)
	except:
		os.mkdir(dest_dir)
	args = []
	args.append("../../src/rsvndump")
	for i in range(0, len(extra_args)):
		args.append(extra_args[i])
	url = "file://"+os.getcwd()+"/"+repos_dir+"/"+test_id
	if subdir:
		url += "/"+subdir()
	args.append(url)
	redir = {}
	redir['output'] = dump_dir+"/"+test_id+"/"+"rsvndump.dump"
	redir['error'] = log_dir+"/"+test_id
	run(*tuple(args), **redir)


def rsvndump_load():
	print(">> Importing dumpfile...")
	repos = repos_dir+"/"+test_id+".tmp"
	run("svnadmin", "create", repos)
	run("svnadmin", "load", repos, input=dump_dir+"/"+test_id+"/rsvndump.dump", output=log_dir+"/"+test_id)


def rsvndump_diff():
	print ">> Validating...",
	run("svnadmin", "dump", repos_dir+"/"+test_id+".tmp", output=dump_dir+"/"+test_id+"/"+"validate.dump", error=log_dir+"/"+test_id)
	try:
		run("diff", "-Naur", dump_dir+"/"+test_id+"/original.dump", dump_dir+"/"+test_id+"/validate.dump", output=log_dir+"/"+test_id+".diff")
		print(" ok")
	except:
		print(" failed. See "+log_dir+"/"+test_id+".diff for details")

def rsvndump_diff_ref():
	print ">> Validating..."
	try:
		run("diff", "-Naur", dump_dir+"/"+test_id+"/original.dump", dump_dir+"/"+test_id+"/rsvndump.dump", output=log_dir+"/"+test_id+".diff")
		print(" ok")
	except:
		print(" failed. See "+log_dir+"/"+test_id+".diff for details")


# Program entry point
if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("USAGE: runtest.py <test> [extra_args]")
		raise SystemExit(1)
	for i in range(2, len(sys.argv)):
		extra_args.append(sys.argv[i])

	if sys.argv[1].endswith(".py"):
		sys.argv[1] = sys.argv[1][:-3]
	test = __import__(sys.argv[1], None, None, [''])
	print("** Starting test '"+test.info()+"'")
	setup_repos(sys.argv[1], test.modify_tree)

	try:
		fn = test.write_reference_dump
		print ">> Copying reference dump..."
		dest_dir = dump_dir+"/"+test_id
		try:
			os.stat(dest_dir)
		except:
			os.mkdir(dest_dir)
#		try:
		fn(dest_dir+"/"+"original.dump")
#		except:
#			print "!! Failed:", sys.exc_info()[0]
#			raise SystemExit(1)
	except AttributeError:
		preproc = None
		try:
			preproc = test.preprocess_dump
		except:
			pass
		dump_repos(preproc)

	subdir = None
	try:
		subdir = test.dump_dir
	except AttributeError:
		extra_args.append("--dump-uuid")
		pass
	rsvndump_dump(subdir)

	rsvndump_load()

	try:
		fn = test.write_reference_dump
		rsvndump_diff_ref()
	except AttributeError:
		rsvndump_diff()
