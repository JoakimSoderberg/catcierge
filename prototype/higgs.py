#
# This file is part of the Catcierge project.
#
# Copyright (c) Joakim Soderberg 2013-2014
#
#    Catcierge is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 2 of the License, or
#    (at your option) any later version.
#
#    Catcierge is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Catcierge.  If not, see <http://www.gnu.org/licenses/>.
#
import cv2
import numpy as np
import random
import sys
import os
import glob
import argparse
#from matplotlib import pyplot as plt
import signal
import sys
from subprocess import call

def signal_handler(signal, frame):
        print 'You pressed Ctrl+C!'
        sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

###################################################

parser = argparse.ArgumentParser()

parser.add_argument("--images", metavar = "IMAGES", nargs = "+",
                    help = "The Catcierge match images to test.",
                    default = glob.glob("../examples/higgs*.png"))

parser.add_argument("--snout", metavar = "SNOUTIMAGE", 
                    help = "The snout image.",
                    default = "../examples/snout/snout320x240.png")

parser.add_argument("--output", metavar = "PATH",
                    help = "Output directory.",
                    default = "output")

parser.add_argument("--noshow", action = "store_true",
                    help = "Show output.")

parser.add_argument("--threshold", metavar = "THRESHOLD",
                    help = "Match threshold.", type = float,
                    default = 0.81)

parser.add_argument("--montage", action = "store_true",
                    help = "Create montage of final match images using imagemagick.")

args = parser.parse_args()

def do_the_match(simg, snout_image):
    # We only deal in grayscale.
    #simg = cv2.resize(img, (0, 0), fx = 0.5, fy = 0.5)
    simg_gray = cv2.cvtColor(simg, cv2.COLOR_BGR2GRAY)
    
    cv2.imwrite("%s/%s_01original%s" % (args.output, filename, fileext), simg)
    cv2.imwrite("%s/%s_02gray%s" % (args.output, filename, fileext), simg_gray)
    
    # Threshold.
    ret, threshimg = cv2.threshold(simg_gray, 90, 255, 0)
    if not args.noshow:
        cv2.imshow('Binary', threshimg)
    cv2.imwrite("%s/%s_03binary%s" % (args.output, filename, fileext), threshimg)

    # Noise removal.
    nonoise_img = cv2.erode(threshimg, kernel, iterations = 3)
    if not args.noshow:
        cv2.imshow('No noise', nonoise_img)
    cv2.imwrite("%s/%s_04nonoise%s" % (args.output, filename, fileext), nonoise_img)

    # Match the snout with the binary noise reduced image.
    matchres = cv2.matchTemplate(nonoise_img, snout_image, cv2.TM_CCOEFF_NORMED)
    (min_x, max_x, minloc, maxloc) = cv2.minMaxLoc(matchres)
    (x, y) = maxloc

    if not args.noshow:
        cv2.imshow("match", matchres)
    cv2.imwrite("%s/%s_05tempmatch%s" % (args.output, filename, fileext), matchres)

    # From some testing 0.9 seems like a good threshold.
    if (max_x >= args.threshold):
        print "Match! %s" % max_x
        color = (0, 255, 0)
    else:
        print "No match! %s" % max_x
        color = (0, 0, 255)

    # Draw the best match.
    snout_w = snout_image.shape[1]
    snout_h = snout_image.shape[0]
    cv2.rectangle(simg, (x, y), (x + snout_w, y + snout_h), color, 2)

    # Draw the final image.
    if not args.noshow:
        cv2.imshow('Final', simg)
    cv2.imwrite("%s/%s_06final%s" % (args.output, filename, fileext), simg)

    """
    plt.subplot(231),plt.imshow(simg,'gray'),plt.title('ORIGINAL')
    plt.subplot(232),plt.imshow(simg_gray,'gray'),plt.title('GRAY')
    plt.subplot(233),plt.imshow(threshimg,'gray'),plt.title('BINARY')
    plt.subplot(234),plt.imshow(nonoise_img,'gray'),plt.title('NO-NOISE')
    plt.subplot(235),plt.imshow(matchres,'gray'),plt.title('MATCH RESULT')
    plt.subplot(236),plt.imshow(simg,'gray'),plt.title('MATCH')
    """

    return max_x



# Read the snout image and make it binary and nice.
org_snout = cv2.imread(args.snout, 0)
ret, threshimg = cv2.threshold(org_snout, 35, 255, 0)
kernel = np.ones((1,1), np.uint8)
snout = threshimg
snout_flipped = cv2.flip(snout, 1)
#snout = cv2.erode(threshimg, kernel, iterations = 3)
#snout = cv2.resize(snout, (0, 0), fx = 0.5, fy = 0.5)
if not args.noshow:
    cv2.imshow('Snout', snout)
    cv2.waitKey(0)

if not os.path.exists(args.output):
    os.makedirs(args.output)

for infile in args.images:
    path, filename_wext = os.path.split(infile)
    filename, fileext = os.path.splitext(filename_wext)
    img = cv2.imread(infile)
    img2 = img.copy()

    res = do_the_match(img, snout)

    if res < args.threshold:
        print "Flipping"
        do_the_match(img2, snout_flipped)

    if not args.noshow:
        cv2.waitKey(0)
    
    cv2.destroyAllWindows()

if args.montage:
    snout_path, snout_filename_wext = os.path.split(args.snout)
    snout_filename, snout_fileext = os.path.splitext(snout_filename_wext)

    montage_args = glob.glob("%s/*final.png" % args.output)
    montage_filename = "%s_montage.jpg" % snout_filename

    # First create the montage of all matches.
    call(["montage", "-background", "black"] 
        + glob.glob("%s/*final.png" % args.output) 
        + [montage_filename])

    # Then include the snout image first.
    call(["montage", 
        "-geometry", "+2+2",
        "-tile", "1x2",
        "-title", "Threshold %s" % args.threshold,
        "-fill", "white",
        "-background", "black", args.snout, montage_filename, 
        "combined_%s_montage.jpg" % snout_filename])
