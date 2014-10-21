#! /usr/bin/env python
import subprocess
import sys


def main():
	def run(*args, **kwargs):
		input = kwargs.pop("input", None)
		output = kwargs.pop("output", None)
		assert not kwargs
		redirections = {}
		if input:
			redirections["stdin"] = open(input)
		if output:
			redirections["stdout"] = open(output, "w")
		# print args, redirections
		subprocess.check_call(args, **redirections)

	domain, problem, result_name = sys.argv[1:]

	# run translator
	run("translate/translate.py", domain, problem)

	# run preprocessing
	run("preprocess/preprocess", input="output.sas")

	# run search
	run("search/search", "yY", "g", result_name, input="output")

if __name__ == "__main__":
    main()

