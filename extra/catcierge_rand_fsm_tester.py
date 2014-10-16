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
	rand_file = random.choice(files)  # .../all/match__2014-09-23_17_15_10__1.png
	file_glob = "%s*.png" % rand_file[:-5]  # .../all/match__2014-09-23_17_15_10__

	#fsm_args = "%s --images %s" % (args.extra_args, file_glob)

	the_args = [args.exe_path] + args.extra_args.split(" ") + ["--images"] + glob.glob(file_glob)
	print(the_args)

	call(the_args)
	#"--input ../extra/templates/*.json --output_path ~/higgs/catcierge_images/%match_group_id% --match_output_path %output_path%/%matchcur_id% --zmq --new_execute"



if __name__ == '__main__': main()
