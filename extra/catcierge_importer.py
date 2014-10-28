#!/usr/bin/python
import argparse
from subprocess import call
import time
import datetime
import os
import random
import glob
import re

from datetime import date, datetime, timedelta

def datespan(startDate, endDate, delta=timedelta(days=1)):
	currentDate = startDate
	while currentDate < endDate:
		yield currentDate
		currentDate += delta

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--exe_path", help="Path to catcierge_fsm_tester executable")
	parser.add_argument("--image_dir", help="Directory of images")
	parser.add_argument("--extra_args", help="Extra arguments to pass", default="")

	args = parser.parse_args()

	extra_args = [os.path.expanduser(os.path.expandvars(s)) for s in args.extra_args.split(" ")]

	firsts = glob.glob("%s/match*__0.png" % args.image_dir)

	while len(firsts) > 0:

		filepath = firsts[0]

		if not filepath:
			break

		filedir = os.path.dirname(filepath)
		filename = os.path.basename(filepath).lstrip("match_").lstrip("_").lstrip("fail")
		print("%s" % filepath)

		# Get the date part of the filename.
		# From: match__2014-06-08_15_10_03__0.png
		# Get:  2014-06-08_15_10_03
		m = re.search(".*?((\d+)-(\d+)-(\d+)_(\d+)_(\d+)_(\d+)).*?\.png", filename)
		
		if m:
			org_timestr = m.group(1)

			# Parse the datetime from the filename date string.
			tm = time.strptime(org_timestr, "%Y-%m-%d_%H_%M_%S")
			dt = datetime.fromtimestamp(time.mktime(tm))

			# Try to find 4 without generating timestamps first.
			candidates = []
			candidates.extend(glob.glob("match*%s*__[0-3].png" % org_timestr))
			num_cands = len(candidates)

			if (num_cands < 4):
				# We failed to find enough candidates using the same timestamp!
				#
				# Now generate datetime strings of the same format for 3 seconds forward
				# with a 1 second step.
				# From:     2014-06-08_15_10_03
				# 
				# Generate: 2014-06-08_15_10_04
				#           2014-06-08_15_10_05
				#           2014-06-08_15_10_06
				#           2014-06-08_15_10_07
				#           2014-06-08_15_10_08
				#
				# And look for any filenames in the same directory as the original file
				# with these datetimes in the filename:
				# glob.glob("*2014-06-08_15_10_04*.png")
				#
				for timestamp in datespan(dt, dt + timedelta(seconds=3), delta=timedelta(seconds=1)):
					timestr = timestamp.strftime("%Y-%m-%d_%H_%M_%S")
					candidates.extend(glob.glob("%s/*%s*__[0-3].png" % (filedir, timestr)))

				if len(candidates) > 4:
					print("WARNING: Got more than 4 candidates: %s" % candidates)
					# TODO: Copy these images to a separate directory for manual processing
				else:
					print [os.path.basename(x) for x in candidates]
					# --exe_path bin/catcierge_fsm_tester --image_dir ../examples/real/all/ --extra_args "--input ../extra/templates/[event]event_%time%.json --output_path ~/higgs/catcierge_images/%match_group_id% --match_output_path %output_path%/%matchcur_id% --steps_output_path %match_output_path%/steps --zmq --new_execute --save --save_steps --save_obstruct --delay 1
					if (args.exe_path):
						print("=" * 79)
						the_args = [args.exe_path] + extra_args + ["--images"] + candidates + ["--base_time", dt.strftime("%Y-%m-%dT%H:%M:%S")]
						print(the_args)
						call(the_args)
						
		else:
			print("Skipping invalid file: %s" % filename)

		firsts.pop(0)


if __name__ == '__main__': main()
