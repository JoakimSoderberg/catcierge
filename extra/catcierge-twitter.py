#!/usr/bin/python
from twython import Twython
import argparse
from subprocess import call
import time
import datetime

#
# Note that this program needs Twython and Imagemagick installed to work!
#

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--images", metavar = "IMAGES", nargs = "+",
					help = "The Catcierge match images to send.")

	parser.add_argument("--consumer_key", metavar = "CONSUMER_KEY", 
					help = "The twitter Consumer Key to use for authentication")

	parser.add_argument("--consumer_secret", metavar = "CONSUMER_KEY",
					help = "The twitter Consumer Secret to use for authentication")

	parser.add_argument("--oauth_token", metavar = "OAUTH_TOKEN",
					help = "Oauth token")

	parser.add_argument("--oauth_token_secret", metavar = "OAUTH_TOKEN_SECRET",
					help = "Ouauth token secret")

	parser.add_argument("--status", metavar = "STATUS", type = int,
					help = "The status of the match, 0 = failure, 1 = success.")

	parser.add_argument("--match_statuses", metavar="MATCHSTATUS", type = float, nargs="+",
					help = "List of statuses for each match", default = [])

	parser.add_argument("--direction", metavar="DIRECTION",
					help = "0 for in, 1 for out, -1 for unknown", default = -1)

	args = parser.parse_args()
	
	twitter = Twython(args.consumer_key, args.consumer_secret, args.oauth_token, args.oauth_token_secret)

	# Merge the images into a 4x4 grid using ImageMagick montage.
	montage_args = "-fill white -background black -geometry +2+2 -mattecolor %s" % ("green" if args.status else "red")
	
	i = 0
	for img in args.images:
		montage_args += ' -frame 2 -label "%s" %s' % (args.match_statuses[i], img)
		i += 1

	comp_img = datetime.datetime.fromtimestamp(int(time.time())).strftime('comp_%Y-%m-%d_%H-%M-%S.jpg')
	montage_args += " %s" % comp_img
	call(["montage"] + montage_args.split(" "))

	img = open(comp_img, 'rb')

	direction = "UNKNOWN"

	try:
		if int(args.direction) == 1:
			direction = "OUT"
		elif int(args.direction) == 0:
			direction = "IN"
	except:
		direction = args.direction.upper()

	twitter.update_status_with_media(status = 'Match: %s\nDirecton: %s\n' % (("OK" if args.status else "FAIL"), direction), media = img)


if __name__ == '__main__': main()

