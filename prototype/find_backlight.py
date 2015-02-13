#!/usr/bin/python

import numpy as np
import cv2
import sys
import os
import glob
import argparse

parser = argparse.ArgumentParser()

parser.add_argument("images", metavar = "IMAGES", nargs = 1,
                    help = "The Catcierge match images to test.")

args = parser.parse_args()

img = cv2.imread(args.images[0])
img_gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
ret, threshimg = cv2.threshold(img_gray, 100, 255, 0)

contours, hierarchy = cv2.findContours(threshimg, cv2.RETR_LIST, cv2.CHAIN_APPROX_SIMPLE)
max_area = 0
max_contour = None
for contour in contours:
	area = cv2.contourArea(contour)
	if (area > max_area):
		max_area, max_contour = area, contour

r = cv2.boundingRect(max_contour)
print r
x, y, w, h = r
cv2.drawContours( img, contours, -1, (0, 255, 0), 3 )
cv2.rectangle(img, (x, y), (x + w, y + h), (255, 0, 0), 1)

cv2.imwrite("sq.png", img)
cv2.imwrite("thr.png", threshimg)
