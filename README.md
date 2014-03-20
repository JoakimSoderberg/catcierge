[![Build Status](https://travis-ci.org/JoakimSoderberg/catcierge.png)](https://travis-ci.org/JoakimSoderberg/catcierge)
[![Coverage Status](https://coveralls.io/repos/JoakimSoderberg/catcierge/badge.png)](https://coveralls.io/r/JoakimSoderberg/catcierge)

Catcierge
=========
![Catcierge](https://raw2.github.com/JoakimSoderberg/catcierge-examples/master/diy/small_logo.jpg)

An image recognition and RFID cat chip solution for detecting "gifts" and
neighbour cats for an automated cat door system. Designed for use with the
Raspberry Pi including camera board.

Background
----------
The Catcierge project came about to solve the problem of our cat having the
nasty habit of delivering "gifts" to us through our cat door in the form 
of dead, or partly dead / fully alive rodents or birds.

Instead of simply not allowing our cat to use the cat door like normal people
I of course set out to find a high-tech solution to solve my perdicament.

My idea was to somehow use image recognition to lock the cat door if it had
something in it's mouth. My first idea was to use Haar Cascades as is normally
used for facial recognition, but instead train it to work on cats.

In the process of researching possible solutions I found the 
[Flo Control project][flo_control] that instead uses a controlled envrionment
to get a good profile image of the cat where the "gift" easily can be
distingusihed. I decided to implement this idea instead. 

Hardware design details
-----------------------
To read more about how to build your own hardware that this code can run on, and the development story, see the webpage: [http://joakimsoderberg.github.io/catcierge/](http://joakimsoderberg.github.io/catcierge/)

Dependencies
------------
For the image recognition catcierge users OpenCV using the 
[raspicam_cv library][raspicam_cv] written by [Emil Valkov][emil_valkov]
(which is included in the catcierge source).

### Raspberry pi

To install OpenCV on raspbian:

```bash
$ sudo apt-get install cmake opencv-dev build-essential
```

To get the raspicam_cv libary to compile we need the Raspberry pi 
userland tools:

```bash
$ git clone https://github.com/raspberrypi/userland.gi
$ cd userland
$ ./buildme
$ pwd # Save this path for compiling.
```

### Linux / OSX

Use your favorite package system to install OpenCV.

You can also use your own build of OpenCV when compiling:

from git [https://github.com/itseez/opencv](https://github.com/itseez/opencv)

or download: [http://opencv.org/downloads.html](http://opencv.org/downloads.html)

### Windows

Download OpenCV 2.x for Windows: [http://opencv.org/downloads.html](http://opencv.org/downloads.html)

Unpack it to a known path (you need this when compiling).

Compiling
---------
Catcierge uses the CMake build system. To compile:

### Raspbian:

```bash
$ git clone <url>
$ cd catcierge
$ mkdir build && cd build
$ cmake -DRPI_USERLAND=/path/to/rpi/userland ..
$ make
```

If you don't have any [RFID cat chip reader][rfid_cat] you can exclude
it from the compilation:

```bash
$ cmake -DRPI_USERLAND=/path/to/rpi/userland -DWITH_RFID=OFF ..
```

### Linux / OSX

```bash
$ git clone <url>
$ cd catcierge
$ mkdir build && cd build
$ cmake -DRPI=0 ..
$ cmake --build .
```

If OpenCV is not automatically found, build your own (See above for downloads)
and point CMake to that build:

```bash
... # Same as above...
$ cmake -DRPI=0 -DOpenCV_DIR=/path/to/opencv/build .. # This should be the path containing OpenCVConfig.cmake
$ cmake --build .
```

### Windows

Assuming you're using [git bash](http://git-scm.com/) and [Visual Studio Express](http://www.visualstudio.com/downloads/download-visual-studio-vs) (or more advanced version).

```bash
$ git clone <url>
$ cd catcierge
$ mkdir build && cd build
$ cmake -DOpenCV_DIR=/c/PATH/TO/OPENCV/build .. # The OpenCV path must contain OpenCVConfig.cmake
$ cmake --build .     # Either build from command line...
$ start catcierge.sln # Or launch Visual Studio and build from there...
```
Running
-------
The main program is named [catcierge_grabber](catcierge_grabber.c) which 
performs all the logic of doing the image recognition, RFID detection and
deciding if the door should be locked or not.

For more help on all the settings:

```bash
$ ./catcierge_grabber --help
```

To test the image recognition there is a test program 
[catcierge_tester](catcierge_tester.c) that allows you to specify an image
to match against.

```bash
$ ./catcierge_tester --snout /path/to/image/of/catsnout.png --images *.png
```

Likewise for the RFID matching:

```bash
$ ./catcierge_rfid_tester
```

To test different matching strategies there's a Python prototype as well
in the aptly named "protoype/" directory. The prototype is named after my
cat [higgs.py](prototype/higgs.py). 

It has some more advanced options that allows you to create montage 
pictures of the match result of multiple images. This was used to
compare the result of different matching strategies during development.

For this to work you will need to have [ImageMagick][imagemagick] installed.
Specifically the program `montage`.


```bash
$ cd prototype/
$ python higgs.py --help
```

Test a load of test images and create a montage from them using two
snout images to do the match:
(Note that it is preferable if you clear the output directory before
creating the montage, so outdated images won't be included).

```bash
$ rm -rf <path/to/output> # Clear any old images.
$ python higgs.py --snout snouts/snout{1,2}.png --output <path/to/output> --noshow --threshold 0.8 --avg --montage
```

[imagemagick]: http://www.imagemagick.org/
[flo_control]: http://www.quantumpicture.com/Flo_Control/flo_control.htm]
[raspicam_cv]: https://github.com/robidouille/robidouille/tree/master/raspicam_cv
[emil_valkov]: http://www.robidouille.com/
[rfid_cat]: http://www.priority1design.com.au/shopfront/index.php?main_page=product_info&cPath=1&products_id=23
