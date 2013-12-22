import cv2
import numpy as np
import random
import sys
import os
import glob

def resize_and_gray_image(img):
    simg = cv2.resize(img, (0,0), fx=0.5, fy=0.5)
    simg_gray = cv2.cvtColor(simg, cv2.COLOR_BGR2GRAY)
    return simg, simg_gray

def get_contours(img_gray):
    ret, threshimg = cv2.threshold(simg_gray, 35, 255, 0)# cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)

    # Noise removal.
    kernel = np.ones((3,3),np.uint8)
    nonoise_img = cv2.erode(threshimg, kernel, iterations = 3)

    # findContours changes the image, so make a copy.
    thresh_img = nonoise_img.copy()

    contours, hierarchy = cv2.findContours(nonoise_img, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    return (thresh_img, contours, hierarchy)

###################################################

for infile in glob.glob('HIGGS/higgs*.png'):

    img = cv2.imread(infile)
    snout = cv2.imread('HIGGS/snout3.png', 0)

    simg, simg_gray = resize_and_gray_image(img)
    thresh_img, contours, hierarchy = get_contours(simg_gray)
    cv2.imshow('Binary', thresh_img)

    matchres = cv2.matchTemplate(thresh_img, snout, cv2.TM_CCOEFF_NORMED)
    (min_x, max_x, minloc, maxloc) = cv2.minMaxLoc(matchres)
    (x, y) = maxloc

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
    cv2.imshow("match", matchres)

    # Draw the final image.
    cv2.drawContours(simg, contours, -1, color, 3)
    cv2.imshow('Final', simg)

    cv2.waitKey(0)
    cv2.destroyAllWindows()


