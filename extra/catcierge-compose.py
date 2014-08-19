#!/usr/bin/env python
# -*- coding: utf-8 -*-
import argparse
from wand.image import Image
from wand.font import Font
from wand.drawing import Drawing
from wand.color import Color

kernel_size = 20

def draw_kernel(w=3, h=3):
	k = Image(width=w * kernel_size + 2, height=h * kernel_size + 2)
	
	draw = Drawing()
	draw.fill_color = Color("white")
	draw.stroke_color = Color("black")

	for y in xrange(0,h):
		for x in xrange(0,w):
			draw.rectangle(left=x*kernel_size, top=y*kernel_size,
							width=kernel_size, height=kernel_size)
			draw(k)

	return k

def compose_adaptive_prey(img_paths, gap=5, horizontal_gap=5):
	img = Image(width=800, height=1324)

	imgs = []

	for img_path in img_paths:
		print img_path
		imgs.append(Image(filename=img_path))

	mpos = lambda w: (img.width - w) / 2

	font = Font(path="fonts/source-code-pro/SourceCodePro-Medium.otf", size=64)
	font_title = Font(path="fonts/alex-brush/AlexBrush-Regular.ttf", size=64)
	font_math = Font(path="fonts/Asana-Math/Asana-Math.otf", size=64)
	
	kernel3x3 = draw_kernel(w=3, h=3)
	kernel2x2 = draw_kernel(w=2, h=2)
	kernel5x1 = draw_kernel(w=5, h=1)

	x_start = 140

	img.caption("Catcierge", left=(img.width - 250) / 2, top=5, width=250, height=100, font=font_title)

	height = 120

	# Original.
	img.composite(imgs[0], left=mpos(imgs[0].width), top=height) 
	height += imgs[0].height + gap
	
	# Head detected.
	head_row_width = imgs[1].width + horizontal_gap + imgs[2].width
	head_row_height = imgs[1].height
	head_row = Image(width=head_row_width, height=head_row_height)
	
	head_row.composite(imgs[1], left=0, top=0)
	#height += imgs[1].height + gap

	# Cropped roi.
	head_row.composite(imgs[2], left=imgs[1].width + horizontal_gap, top=(imgs[1].height - imgs[2].height) / 2)

	img.composite(head_row, left=mpos(head_row.width), top=height)
	height += head_row_height + gap

	#
	# Threshold images.
	#
	thr_row_height = imgs[3].height
	thr_row_width = img.width # imgs[3].width + imgs[4].width + imgs[5].width + (2 + 8 + 2 + 8 + 8) * horizontal_gap
	thr_row = Image(width=thr_row_width, height=thr_row_height)
	
	# Global threshold.
	x = x_start
	thr_row.composite(imgs[3], left=x, top=0)
	x += imgs[3].width + 5 * horizontal_gap

	thr_row.caption("+", left=x, top=-15, width=150, height=100, font=font)
	x += 10 * horizontal_gap
	
	# Adaptive threshold.
	thr_row.composite(imgs[4], left=x, top=0)
	x += imgs[4].width + 4 * horizontal_gap

	thr_row.caption("=", left=x, top=-15, width=150, height=100, font=font)
	x += 9 * horizontal_gap + 2
	
	# Threshold Combined.
	thr_row.composite(imgs[5], left=x, top=0)
	img.composite(thr_row, left=mpos(thr_row.width), top=height)
	height += imgs[5].height + gap

	#
	# Opened.
	#
	open_row_width = img.width #imgs[5].width + kernel2x2.width + imgs[6].width + (4 + 8 + 4 + 10) * horizontal_gap
	open_row_height = imgs[5].height
	open_row = Image(width=open_row_width, height=open_row_height)

	# Threshold combined.
	x = x_start
	open_row.composite(imgs[5], left=x, top=0)
	x += imgs[5].width + 6 * horizontal_gap

	open_row.caption(u"∘", left=x, top=0, width=100, height=100, font=font_math)
	x += 18 * horizontal_gap

	# 2x2 kernel
	open_row.composite(kernel2x2, left=x, top=(open_row.height - kernel2x2.height) / 2)
	x += kernel2x2.width + 14 * horizontal_gap

	open_row.caption("=", left=x, top=-15, width=150, height=100, font=font)
	x += 10 * horizontal_gap

	# The opened image.
	open_row.composite(imgs[6], left=x, top=0)
	img.composite(open_row, left=mpos(open_row.width), top=height)
	height += imgs[6].height + gap

	#
	# Dilated.
	#
	dilated_row_width = img.width #imgs[6].width + kernel2x2.width + imgs[7].width + (4 + 12 + 10 + 8) * horizontal_gap
	dilated_row_height = imgs[6].height
	dilated_row = Image(width=dilated_row_width, height=dilated_row_height)

	# Threshold combined.
	x = x_start
	dilated_row.composite(imgs[6], left=x, top=0)
	x += imgs[6].width + 4 * horizontal_gap

	dilated_row.caption(u"⊕", left=x, top=0, width=50, height=100, font=font_math)
	x += 18 * horizontal_gap

	# 3x3 kernel
	dilated_row.composite(kernel3x3, left=x, top=(dilated_row.height - kernel3x3.height) / 2)
	x += kernel3x3.width + 12 * horizontal_gap

	dilated_row.caption("=", left=x, top=-15, width=150, height=100, font=font)
	x += 10 * horizontal_gap

	# The dilated image.
	dilated_row.composite(imgs[7], left=x, top=0)
	img.composite(dilated_row, left=mpos(dilated_row.width), top=height)
	height += imgs[7].height + gap

	# Inverted.
	img.composite(imgs[8], left=mpos(imgs[8].width), top=height)
	height += imgs[8].height + gap

	# Contours.
	img.composite(imgs[9], left=mpos(imgs[9].width), top=height)
	height += imgs[9].height + gap

	# Final.
	img.composite(imgs[10], left=mpos(imgs[10].width), top=height)
	height += imgs[10].height + gap

	img.save(filename="tut.png")

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--images", metavar="IMAGES", nargs="+",
					help="The Catcierge match images to use if no json file is specified.")

	parser.add_argument("--json", metavar="JSON", nargs=1,
					help="")

	args = parser.parse_args()

	image_count = len(args.images)

	if (image_count == 2):
		return
	elif (image_count == 11):
		compose_adaptive_prey(img_paths=args.images, gap=5)
	else:
		print("Invalid number of images %d, expected 2 or 11" % image_count)

if __name__ == '__main__': main()
