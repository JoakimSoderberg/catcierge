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

parser.add_argument("--snout", metavar = "SNOUTIMAGES", 
                    help = "The snout image.", nargs = "+",
                    default = ["../examples/snout/snout320x240.png"])

parser.add_argument("--output", metavar = "PATH",
                    help = "Output directory.",
                    default = "output")

parser.add_argument("--noshow", action = "store_true",
                    help = "Show output.")

parser.add_argument("--threshold", metavar = "THRESHOLD",
                    help = "Match threshold.", type = float,
                    default = 0.8)

parser.add_argument("--erode", action = "store_true",
                    help = "Should we erode the input to get a less noisy image.")

parser.add_argument("--montage", action = "store_true",
                    help = "Create montage of final match images using imagemagick.")

parser.add_argument("--avg", action = "store_true",
                    help = "Use multiple snout images to match to an average.")

args = parser.parse_args()

kernel = np.ones((1,1), np.uint8)

if not os.path.exists(args.output):
        os.makedirs(args.output)

def do_the_match_avg(simg, snout_images, filename, fileext):

    simg_gray = cv2.cvtColor(simg, cv2.COLOR_BGR2GRAY)

    ret, threshimg = cv2.threshold(simg_gray, 90, 255, 0)

    max_x_sum = 0

    for snout_image in snout_images:
        matchres = cv2.matchTemplate(threshimg, snout_image, cv2.TM_CCOEFF_NORMED)
        (min_x, max_x, minloc, maxloc) = cv2.minMaxLoc(matchres)
        (x, y) = maxloc
        max_x_sum += max_x

    max_x_avg = max_x_sum / len(snout_images)

    if (max_x_avg >= args.threshold):
        print "Match! %s" % max_x_avg
        color = (0, 255, 0)
    else:
        print "No match! %s" % max_x_avg
        color = (0, 0, 255)

    # Draw the best match.
    snout_w = snout_image.shape[1]
    snout_h = snout_image.shape[0]
    cv2.rectangle(simg, (x, y), (x + snout_w, y + snout_h), color, 2)

    # Draw the final image.
    if not args.noshow:
        cv2.imshow('Final', simg)
    cv2.imwrite("%s/%s_06final%s" % (args.output, filename, fileext), simg)

    return max_x_avg

def do_the_match(simg, snout_image, filename, fileext):
    # We only deal in grayscale.
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

    if (max_x >= args.threshold):
        print "Match! %s   (%s)" % (max_x, filename)
        color = (0, 255, 0)
    else:
        print "No match! %s   (%s)" % (max_x, filename)
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

def prepare_snout(current_snout):
    # Read the snout image and make it binary and nice.
    org_snout = cv2.imread(current_snout, 0)
    ret, threshimg = cv2.threshold(org_snout, 35, 255, 0)
    snout = threshimg

    if (args.erode):
        snout = cv2.erode(threshimg, kernel, iterations = 3)
    else:
        snout = threshimg

    snout_flipped = cv2.flip(snout, 1)

    if not args.noshow:
        cv2.imshow('Snout', snout)
        cv2.waitKey(0)

    return (snout, snout_flipped)

def create_montage(current_snout, success_count):
    # Creates a montage for all final images.
    snout_path, snout_filename_wext = os.path.split(current_snout)
    snout_filename, snout_fileext = os.path.splitext(snout_filename_wext)

    montage_args = glob.glob("%s/*final.png" % current_snout)
    montage_filename = "%s_montage.jpg" % snout_filename

    # First create the montage of all matches.
    call(["montage", "-background", "black"] 
        + glob.glob("%s/*final.png" % args.output) 
        + [montage_filename])

    # Then include the snout image first.
    call(["montage", 
        "-geometry", "+2+2",
        "-tile", "1x2",
        "-title", "%s of %s successfully matched. %sThreshold %s" % (success_count, len(args.images), "Eroded, " if args.erode else "", args.threshold),
        "-fill", "white",
        "-background", "black", current_snout, montage_filename, 
        "%scombined_%s_montage.jpg" % ("eroded_" if args.erode else "", snout_filename)])


def perform_one_match(current_snout, snout, snout_flipped):
    print("Snout: %s" % current_snout)

    (snout, snout_flipped) = prepare_snout(current_snout)

    success_count = 0

    for infile in args.images:
        path, filename_wext = os.path.split(infile)
        filename, fileext = os.path.splitext(filename_wext)
        img = cv2.imread(infile)
        img2 = img.copy()

        res = do_the_match(img, snout, filename, fileext)

        if res < args.threshold:
            print "Flipping"
            res = do_the_match(img2, snout_flipped, filename, fileext)

        if res >= args.threshold:
            success_count += 1

        if not args.noshow:
            cv2.waitKey(0)
        
        cv2.destroyAllWindows()

    if args.montage:
        create_montage(current_snout, success_count)

###############################################################

if (args.avg):
    # Use multiple snout images and use the average to decide
    # if it's a match or not.
    snouts = []
    snouts_flipped = []
    for current_snout in args.snout:
        (snout, snout_flipped) = prepare_snout(current_snout)
        snouts.append(snout)
        snouts_flipped.append(snout_flipped)

    success_count = 0

    for infile in args.images:
        path, filename_wext = os.path.split(infile)
        filename, fileext = os.path.splitext(filename_wext)
        img = cv2.imread(infile)
        img2 = img.copy()

        res = do_the_match_avg(img, snouts, filename, fileext)

        if res < args.threshold:
            print "Flipping"
            res_flipped = do_the_match_avg(img2, snouts_flipped, filename, fileext)
            res = max(res, res_flipped)

        print "  avg: %s" % res

        if res >= args.threshold:
            success_count += 1

        if not args.noshow:
            cv2.waitKey(0)

        cv2.destroyAllWindows()

    if args.montage:
        create_montage(current_snout, success_count)
else:
    # Loop through each snout and create a montage of all
    # matches for each one.
    for current_snout in args.snout:
        (snout, snout_flipped) = prepare_snout(current_snout)
        perform_one_match(current_snout, snout, snout_flipped)


