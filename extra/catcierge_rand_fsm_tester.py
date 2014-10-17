#!/usr/bin/python
import argparse
from subprocess import call
import time
import datetime
import os
import random
import glob

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--exe_path", help="Path to catcierge_fsm_tester executable")
	parser.add_argument("--image_dir", help="Directory of images")
	parser.add_argument("--extra_args", help="Extra arguments to pass", default="")

	args = parser.parse_args()

	files = glob.glob("%s/*.png" % args.image_dir)  # Get all files in the image dir.
	images = []
	
	while len(images) < 4:
		rand_file = random.choice(files)  # .../all/match__2014-09-23_17_15_10__1.png
		file_glob = "%s*.png" % rand_file[:-5]  # .../all/match__2014-09-23_17_15_10__
		images = glob.glob(file_glob)

	extra_args = [os.path.expanduser(os.path.expandvars(s)) for s in args.extra_args.split(" ")]

	the_args = [args.exe_path] + extra_args + ["--images"] + images
	print(the_args)

	call(the_args)


if __name__ == '__main__': main()
