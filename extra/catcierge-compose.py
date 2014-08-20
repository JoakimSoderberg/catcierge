#!/usr/bin/env python
# -*- coding: utf-8 -*-
import argparse
from wand.image import Image
from wand.font import Font
from wand.drawing import Drawing
from wand.color import Color

kernel_size = 20
font = Font(path="fonts/source-code-pro/SourceCodePro-Medium.otf", size=64)
font_title = Font(path="fonts/alex-brush/AlexBrush-Regular.ttf", size=64)
font_math = Font(path="fonts/Asana-Math/Asana-Math.otf", size=64)

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

def create_row(imgs, offsets, gap, fixed_width=0, caption=None, caption_offset=(0,0)):
	row_width = 0
	i = 0
	row_height = 0

	for img in imgs:
		if isinstance(img, Image):
			row_width += img.width + gap
			row_height = max(img.height, row_height)
		else:
			print i
			row_width += offsets[i][0] + gap
		i += 1

	if fixed_width:
		row_width = fixed_width

	row = Image(width=row_width, height=row_height)

	i = 0
	x = 0

	for img in imgs:
		if isinstance(img, Image):
			row.composite(img, left=x + offsets[i], top=(row_height - img.height) / 2)
			x += img.width + offsets[i] + gap
		else:
			print i
			(offset_x, offset_y, width, font) = offsets[i]
			row.caption(img, left=x + offset_x, top=offset_y, width=250, height=250, font=font)
			x += width + gap
		i += 1

	if caption:
		caption_font = Font(path="fonts/source-code-pro/SourceCodePro-Medium.otf", size=14)
		row_w_caption = Image(width=row_width, height=row_height+20)
		row_w_caption.caption(caption, left=caption_offset[0], top=caption_offset[1],
								width=1450, height=50, font=caption_font)
		row_w_caption.composite(row, left=0, top=20)
		return row_w_caption
	else:
		return row

def compose_adaptive_prey(img_paths, gap=5, horizontal_gap=5):
	img = Image(width=800, height=1124, background=Color("#8A968E"))

	imgs = []

	for img_path in img_paths:
		print img_path
		imgs.append(Image(filename=img_path))

	mpos = lambda w: (img.width - w) / 2
	
	kernel3x3 = draw_kernel(w=3, h=3)
	kernel2x2 = draw_kernel(w=2, h=2)
	kernel5x1 = draw_kernel(w=5, h=1)

	x_start = 140

	img.caption("Catcierge", left=(img.width - 250) / 2, top=5, width=250, height=100, font=font_title)

	height = 120

	# Original.
	img.composite(imgs[0], left=mpos(imgs[0].width), top=height) 
	height += imgs[0].height + gap
	
	# Detected head + cropped region of interest.
	head_row = create_row(imgs[1:3], [0, 0], horizontal_gap, caption="Detected head  Cropped ROI")
	img.composite(head_row, left=mpos(head_row.width), top=height)
	height += head_row.height + gap

	# Combine the threshold images.
	thr_row = create_row([imgs[3], "+", imgs[4], "=", imgs[5]],
						[x_start,
						(4 * horizontal_gap, -15, 14 * horizontal_gap, font),
						0,
						(2 * horizontal_gap, -15, 8 * horizontal_gap, font),
						2 * horizontal_gap],
						horizontal_gap, fixed_width=img.width,
						caption="Global Threshold           Adaptive Threshold       Combined Threshold",
						caption_offset=(140, 0))
	img.composite(thr_row, left=mpos(thr_row.width), top=height)
	height += thr_row.height + gap

	# Open the combined threshold.
	open_row = create_row([imgs[5], u"∘", kernel2x2, "=", imgs[6]],
						[x_start,
						(5 * horizontal_gap, -5, 14 * horizontal_gap, font_math),
						0,
						(21 * horizontal_gap, -15, 10 * horizontal_gap, font),
						19 * horizontal_gap + 3],
						horizontal_gap, fixed_width=img.width,
						caption="Combined Threshold         2x2 Kernel               Opened Image",
						caption_offset=(140, 0))
	img.composite(open_row, left=mpos(open_row.width), top=height)
	height += open_row.height + gap

	# Dilate opened and combined threshold with a kernel3x3.
	dilated_row = create_row([imgs[6], u"⊕", kernel3x3, "=", imgs[7]],
						[x_start,
						(3 * horizontal_gap, -5, 14 * horizontal_gap, font_math),
						0,
						(17 * horizontal_gap, -15, 10 * horizontal_gap, font),
						15 * horizontal_gap + 3],
						horizontal_gap, fixed_width=img.width,
						caption="Opened Image               3x3 Kernel               Dilated Image",
						caption_offset=(140, 0))
	img.composite(dilated_row, left=mpos(dilated_row.width), top=height)
	height += dilated_row.height + gap

	# Inverted image and contour.
	contour_row = create_row(imgs[8:10], [0, 0], horizontal_gap, caption="  Re-Inverted         Contours")
	img.composite(contour_row, left=mpos(contour_row.width), top=height)
	height += contour_row.height + 2 * gap

	# Final.
	img.composite(imgs[10], left=mpos(imgs[10].width), top=height)
	height += imgs[10].height + gap

	

	img.save(filename="tut.png")

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--images", metavar="IMAGES", nargs="+",
					help="The Catcierge match images to use if no json file is specified.")

	# Add support for inputting a json with the paths and stuff.
	parser.add_argument("--json", metavar="JSON", nargs=1,
					help="")

	parser.add_argument

	args = parser.parse_args()

	image_count = len(args.images)

	if (image_count == 2):
		return
	elif (image_count == 11):
		compose_adaptive_prey(img_paths=args.images, gap=5)
	else:
		print("Invalid number of images %d, expected 2 or 11" % image_count)

if __name__ == '__main__': main()
