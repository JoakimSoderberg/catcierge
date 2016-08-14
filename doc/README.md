Getting started (with Raspberry Pi)
===================================

Since this project was initially designed to run on a Raspberry Pi, this guide
will focus on that. But most of this stuff will work just as fine on any other
platform.

This guide includes the same setup I have, with all "extras" installed. If you
don't need RFID or ZMQ support you can turn those off.

Setup the camera with back light
--------------------------------

You need a back light to create a uniform white light to create a good
contrast for the profile picture of the cats head.

It is a good idea to create a corridor with a quite low roof, and build it so
that the cat consistently moves through it at the same distance from the camera,
always with the head fully in front of the back light.

Install Raspbian
----------------

The code should work on any Raspberry Pi distribution, but since
Raspbian is the most common and uses SystemD.

https://www.raspberrypi.org/downloads/raspbian/

I use Raspbian Lite since we don't really need a full desktop.

Install Dependencies
--------------------

```bash
sudo apt-get install cmake opencv-dev build-essential libzmq3-dev
```

This is optional, it is used to communicate with
[catcierge-cam](https://github.com/JoakimSoderberg/catcierge-cam)
There is no CZMQ package on Raspbian so we have to build it ourselves

```bash
$ git clone git@github.com:zeromq/czmq.git
$ cd czmq
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install
```

Get the code and build it
-------------------------

```bash
git clone --recursive https://github.com/JoakimSoderberg/catcierge.git
cd catcierge
./build_userland.sh
mkdir build && cd build
cmake -LH .. # See a list of all build options.
             # Note that you can change the GPIO pins used here for things such
             # as closing the door and turning on the back light.
cmake ..     # -DWITH_ZMQ=OFF if you don't want ZMQ.
make
```

Now build the debian package:

```bash
cpack
ls *.deb # Get the name of the debian package.
sudo dpkg -i catcierge-0.6.4-armhf.deb # And install it (with the right filename of course).
```

Follow the instructions (but skip step 3)

```bash
(Reading database ... 42602 files and directories currently installed.)
Preparing to unpack catcierge-0.6.4-armhf.deb ...
Unpacking catcierge (0.6.4) over (0.6.4) ...
Setting up catcierge (0.6.4) ...
#########################################
Creating user catcierge
#########################################
The system user 'catcierge' already exists. Exiting.
Getting started
---------------

1. Create a system-wide catcierge config file to get started:

  sudo cp /etc/catcierge/catcierge-example.cfg /etc/catcierge/catcierge.cfg

2. To see available options run:

  catcierge_grabber --help

3. And to finally start the service:

  sudo systemctl start catcierge
```

Wait with step 3, we don't want to start the service yet.

Verify the camera orientation
-----------------------------
Depending on how you have placed your camera, it might be upside down, or you
want to configure some other setting for it.

To do this raspberry pi supports the same command line options used by the
`raspivid` program. If you prepend a command line option with `--rpi-` you can
use these with catcierge as well.

So for example to horizontally flip the image you would use `--rpi-hf`.

Another way to pass these `raspivid` command line options is via a config files,
which is preferable. The default location for this config file is under:
`/etc/catcierge/catcierge-rpi.cfg`, see `--help` to override this at runtime.

The contents of this config file consists of a line for each `raspivid`
command line option we want to use. For example to flip the image both
vertically and horizontally because we placed the camera upside down:

```
-hf
-vf
```

Calibrate the backlight
-----------------------
We need to make sure we have a good back light, and that each time we start up
the camera finds it properly.

To help with this it is possible to set the "Region Of Interest (ROI)" that the
camera will use. So if your camera setup doesn't perfectly frame the back light
you can help it find the sub part of the image it is in.

To see the ROI related settings:

```bash
bin/catcierge_grabber --help | grep -C5 roi
```

```bash
--roi X Y WIDTH HEIGHT                   Crop all input image to this region of interest. Cannot be used
                                         together with --auto_roi.
--auto_roi                               Automatically crop to the area covered by the backlight. This
                                         will be done after --startup_delay has ended. Cannot be used
                                         together with --roi.
--auto_roi_thr THRESHOLD                 Set the threshold values used to find the backlight, using a
                                         binary threshold algorithm. Separate each pixel into either
                                         black or white. White if the greyscale value of the pixel is
                                         above the threshold, and black otherwise.
                                         Default value 90
--min_backlight MIN_BACKLIGHT            If --auto_roi is on, this sets the minimum allowed area the
                                         backlight is allowed to be before it is considered broken. If
                                         it is smaller than this, the program will exit. Default 10000.
--save_auto_roi                          Save the image roi found by --auto_roi. Can be useful for
                                         debugging when tweaking the threshold. Result placed in
                                         --output_path.
```

If the back light takes up the entire frame, you don't need to use these.

One option is also to simply set a fixed ROI as defined using `--roi`. However
things in the real world are not really static, the camera might shift slightly
and so on. So the recommended way of doing this is to use `--auto_roi`.

This setting tries to find the back light on each startup automatically.
However it might still need some help with setting the `--auto_roi_thr` value.

So for this purpose there is a test program that helps you calibrate named `catcierge_bg_tester`. This program works both on the command line, or in an
interactive mode with a GUI and slider that gives you visual feedback.

### Command line ##

If you don't have catcierge compiled on any desktop computer with a GUI, simply
use `catcierge_grabber` to do the calibration. Start the program manually
but make it save the *auto_roi* images. And then quit using *ctrl+c* after
it outputs that it has saved them.

```bash
# All other settings are read from /etc/catcierge/catcierge.cfg
catcierge_grabber --save_auto_roi theimage.png --auto_roi_thr 180
```

Now you should have a picture `auto_roi_highlighted.png` in the current directory
copy that to another computer and adjust your threshold and repeat until you
get a good result.

### GUI ###

If you have a desktop environment on your Raspberry Pi, or you have catcierge
compiled on some other computer, you can also run this program interactively.

First take a picture using `raspistill` of what your camera sees. Or simply start
`catcierge_grabber` as described above, and use the `auto_roi.png` )without the
highlight stuff). Copy it to where you have `catcierge_bg_tester`.

```bash
catcierge_bg_tester --interactive auto_roi.png
```

This will let you tweak the setting in a GUI interactively.

Starting the service
--------------------

When you have everything setup you can then start the service. At the start
of running your cat door you probably don't want to perform a real lockout
until you are sure things work as they should. So a recommendation is to enable
the option `--lockout_dummy` (`lockout_dummy=1` in the config file).

To start the service:

```bash
sudo systemctl start catcierge
```

Then to follow the log and see that things are running fine:

```bash
sudo journalctl -f -u catcierge
```
