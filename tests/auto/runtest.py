#!/usr/bin/env python
#
#	rsvndump test suite
#


import os
import sys
import subprocess


# Some globals
work_dir = "work"
repos_dir = "work/repos"
wc_dir = "work/wc"
dump_dir = "work/dumps"
log_dir = "work/logs"
test_id = ""
extra_args = ""


def run(*args, **redir):
	redirections = {}
	input = redir.pop("input", None)
	output = redir.pop("output", None)
	error = redir.pop("error", None)
	if input:
		redirections['stdin'] = open(input, "r")
	if output:
		redirections['stdout'] = open(output, "a+")
	if error:
		redirections['stderr'] = open(error, "a+")
	subprocess.check_call(args, **redirections)

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
	test_id = name+str(num)
	current_repos = repos_dir+"/"+test_id
	while 1:
		try:
			os.stat(current_repos)
		except:
			break
		num += 1
		test_id = name+str(num)
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
	while modify(wc_dir+"/"+test_id, step):
		os.system("svn commit -m 'commit step "+str(step)+"'")
		step += 1
	os.chdir(old_dir)


def dump_repos():
	print(">> Creating reference dump...")
	dest_dir = dump_dir+"/"+test_id
	try:
		os.stat(dest_dir)
	except:
		os.mkdir(dest_dir)
	run("svnadmin", "dump", repos_dir+"/"+test_id, output=dump_dir+"/"+test_id+"/"+"original.dump", error=log_dir+"/"+test_id)


def rsvndump_dump():
	print(">> Running rsvndump...")
	dest_dir = dump_dir+"/"+test_id
	try:
		os.stat(dest_dir)
	except:
		os.mkdir(dest_dir)
	run("../../src/rsvndump", extra_args, "file://"+os.getcwd()+"/"+repos_dir+"/"+test_id, output=dump_dir+"/"+test_id+"/"+"rsvndump.dump", error=log_dir+"/"+test_id)


def rsvndump_load():
	print(">> Importing dumpfile...")
	repos = repos_dir+"/"+test_id+".tmp"
	run("svnadmin", "create", repos)
	run("svnadmin", "load", repos, input=dump_dir+"/"+test_id+"/rsvndump.dump")


def rsvndump_diff():
	print(">> Validating...")
	run("svnadmin", "dump", repos_dir+"/"+test_id+".tmp", output=dump_dir+"/"+test_id+"/"+"validate.dump", error=log_dir+"/"+test_id)
	run("diff", "-Naur", dump_dir+"/"+test_id+"/original.dump", dump_dir+"/"+test_id+"/validate.dump")


# Program entry point
if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("USAGE: runtest.py <test> [extra_args]")
		raise SystemExit(1)
	extra_args += "--dump-uuid "
	extra_args = extra_args.strip()

	test = __import__(sys.argv[1], None, None, [''])
	print("** Starting test '"+test.info()+"'")
	setup_repos(sys.argv[1], test.modify_tree)
	dump_repos()
	rsvndump_dump()
	rsvndump_load()
	rsvndump_diff()
