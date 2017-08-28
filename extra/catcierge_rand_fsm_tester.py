#!/usr/bin/python
import argparse
from subprocess import call
import time
import datetime
import os
import random
import glob

"""
Example command line (Get a random fail match group):

python ../extra/catcierge_rand_fsm_tester.py 
	--exe_path bin/catcierge_fsm_tester 
	--image_dir ../examples/real/all/
	--fail

	--input ../extra/templates/event.json 
	 --output_path ~/higgs/catcierge_images/%match_group_id%
	 --match_output_path %output_path%/%matchcur_id%
	 --steps_output_path %match_output_path%/steps
	 --zmq
	 --save
	 --save_steps
	 --save_obstruct
	 --delay 1
	 --base_time 2017-08-02T14:00:33" 

python ../extra/catcierge_rand_fsm_tester.py --exe_path bin/catcierge_fsm_tester --image_dir ../examples/real/all/ --input ../extra/templates/event.json --output_path ~/higgs/catcierge_images/%match_group_id% --match_output_path %output_path%/%matchcur_id% --steps_output_path %match_output_path%/steps --zmq --save --save_steps --save_obstruct --delay 1 --base_time 2017-08-02T14:00:33 --fail

"""

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--exe_path", help="Path to catcierge_fsm_tester executable")
	parser.add_argument("--image_dir", help="Directory of images")
	parser.add_argument("--fail", help="Only get a random fail image", action="store_true")
	parser.add_argument("--ok", help="Only get a random ok image", action="store_true")

	args, unknown = parser.parse_known_args()

	# Get all files in the image dir.
	if args.fail:
		files = glob.glob("%s/*fail*.png" % args.image_dir)
	elif args.ok:
		files = glob.glob("%s/*ok*.png" % args.image_dir)
	else:
		files = glob.glob("%s/*.png" % args.image_dir)

	images = []
	
	# Sometimes this method fails to get 4 images.
	# Instead just continue until we get that... 
	while len(images) < 4:
		rand_file = random.choice(files)  # .../all/match__2014-09-23_17_15_10__1.png
		file_glob = "%s*.png" % rand_file[:-5]  # .../all/match__2014-09-23_17_15_10__
		images = glob.glob(file_glob)

	# Any remaining arguments are passed on to catcierge_grabber
	the_args = [args.exe_path] + ["--images"] + images + unknown
	print(the_args)

	call(the_args)


if __name__ == '__main__': main()
