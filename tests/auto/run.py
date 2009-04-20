#!/usr/bin/env python
#
#	rsvndump test suite
#


import subprocess


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
