#!/usr/bin/env python3
#
#	Test database for rsvndump
#	written by Jonas Gehring
#


import os, sys, subprocess

import cache
import test


# Prints usage help
def print_help(cmd = None):
	if cmd == "list":
		print("USAGE: "+sys.argv[0]+" list\n")
		print("Lists all available tests")
	elif cmd == "run":
		print("USAGE: "+sys.argv[0]+" run <test> [args]\n")
		print("Runs the specified test with the given extra arguments")
	elif cmd == "all":
		print("USAGE: "+sys.argv[0]+" all [args]\n")
		print("Runs all tests with the given extra arguments")
	elif cmd == "clear":
		print("USAGE: "+sys.argv[0]+" clear [--cache]\n")
		print("Clears logs, dumps and repositories of previous tests")
		print("If --cache is given, also clear the repository cache")
	else:
		print("USAGE: "+sys.argv[0]+" <action> [options]\n")
		print("action is one of:")
		print("    list    lists available tests")
		print("    run     runs a test")
		print("    all     runs all test")
		print("    clear   clears temporary data")
		print("\nRun "+sys.argv[0]+" help <action> for more specific help")


# Runs a series of tests
def runtests(tests, args):
	ret = 0
	for t in tests:
		tid = test.id(t)
		if not tid:
			continue
		try:
			targs = list(args)
			if test.run(t, tid, targs):
				print("OK     : "+test.info(t)+" ["+tid+"]");
			else:
				print("FAIL   : "+test.info(t)+" ["+tid+"]");
				ret = 1
		except:
			print("EXCEPTION while running test with ID: "+tid);
			raise
	return ret


# Program entry point
def main():
	# Parse arguments
	if len(sys.argv) < 2:
		print_help()
		return 1
	action = sys.argv[1]

	if action == "help":
		if len(sys.argv) > 2:
			print_help(sys.argv[2])
		else:
			print_help()
		return 0
	elif action == "list":
		for t in test.all_tests():
			print(t+" - "+test.info(t))
	elif action == "run":
		args = []
		tests = [sys.argv[2]]
		if len(sys.argv) > 3:
			args = sys.argv[3:]
		return runtests(tests, args)
	elif action == "all":
		args = []
		tests = test.all_tests()
		if len(sys.argv) > 2:
			args = sys.argv[2:]
		return runtests(tests, args)
	elif action == "clear":
		test.cleanup()
		if len(sys.argv) > 2 and sys.argv[2] == "--cache":
			cache.clear()
	else:
		print("Unkown command "+action)
		return 1
	return 0


if __name__ == "__main__":
	ret = main()
	raise SystemExit(ret)
