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
#from matplotlib import pyplot as plt

###################################################

# Read the snout image and make it binary and nice.
org_snout = cv2.imread("../snout1080p.png", 0)
ret, threshimg = cv2.threshold(org_snout, 35, 255, 0)
kernel = np.ones((3,3), np.uint8)
snout = cv2.erode(threshimg, kernel, iterations = 3)
snout = cv2.resize(snout, (0, 0), fx = 0.5, fy = 0.5)

if not os.path.exists("output/"):
    os.makedirs("output/")

for infile in glob.glob("../examples/higgs*.png"):
    path, filename_wext = os.path.split(infile)
    filename, fileext = os.path.splitext(filename_wext)
    img = cv2.imread(infile)

    # We only deal in grayscale.
    simg = cv2.resize(img, (0, 0), fx = 0.5, fy = 0.5)
    simg_gray = cv2.cvtColor(simg, cv2.COLOR_BGR2GRAY)
    
    cv2.imwrite("output/%s_01original%s" % (filename, fileext), simg)
    cv2.imwrite("output/%s_02gray%s" % (filename, fileext), simg_gray)
    
    # Threshold.
    ret, threshimg = cv2.threshold(simg_gray, 35, 255, 0)
    cv2.imshow('Binary', threshimg)
    cv2.imwrite("output/%s_03binary%s" % (filename, fileext), threshimg)

    # Noise removal.
    nonoise_img = cv2.erode(threshimg, kernel, iterations = 3)
    cv2.imshow('No noise', nonoise_img)
    cv2.imwrite("output/%s_04nonoise%s" % (filename, fileext), nonoise_img)

    # Match the snout with the binary noise reduced image.
    matchres = cv2.matchTemplate(nonoise_img, snout, cv2.TM_CCOEFF_NORMED)
    (min_x, max_x, minloc, maxloc) = cv2.minMaxLoc(matchres)
    (x, y) = maxloc

    cv2.imshow("match", matchres)
    cv2.imwrite("output/%s_05tempmatch%s" % (filename, fileext), matchres)

    # From some testing 0.9 seems like a good threshold.
    if (max_x >= 0.9):
        print "Match! %s" % max_x
        color = (0, 255, 0)
    else:
        print "No match! %s" % max_x
        color = (0, 0, 255)

    # Draw the best match.
    snout_w = snout.shape[1]
    snout_h = snout.shape[0]
    cv2.rectangle(simg, (x, y), (x + snout_w, y + snout_h), color, 0)

    # Draw the final image.
    cv2.imshow('Final', simg)
    cv2.imwrite("output/%s_06final%s" % (filename, fileext), simg)

    """
    plt.subplot(231),plt.imshow(simg,'gray'),plt.title('ORIGINAL')
    plt.subplot(232),plt.imshow(simg_gray,'gray'),plt.title('GRAY')
    plt.subplot(233),plt.imshow(threshimg,'gray'),plt.title('BINARY')
    plt.subplot(234),plt.imshow(nonoise_img,'gray'),plt.title('NO-NOISE')
    plt.subplot(235),plt.imshow(matchres,'gray'),plt.title('MATCH RESULT')
    plt.subplot(236),plt.imshow(simg,'gray'),plt.title('MATCH')
    """
    
    cv2.waitKey(0)
    cv2.destroyAllWindows()


