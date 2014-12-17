[![Travis Build Status](https://travis-ci.org/JoakimSoderberg/catcierge.png)](https://travis-ci.org/JoakimSoderberg/catcierge)
[![Drone.io Build Status](https://drone.io/github.com/JoakimSoderberg/catcierge/status.png)](https://drone.io/github.com/JoakimSoderberg/catcierge/latest)
[![Circle CI](https://circleci.com/gh/JoakimSoderberg/catcierge.svg?style=svg)](https://circleci.com/gh/JoakimSoderberg/catcierge)
[![Appveyor Build status](https://ci.appveyor.com/api/projects/status/6aq2tpajh1nmy6b3)](https://ci.appveyor.com/project/JoakimSoderberg/catcierge)
[![Coverage Status](https://coveralls.io/repos/JoakimSoderberg/catcierge/badge.png)](https://coveralls.io/r/JoakimSoderberg/catcierge)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/2506/badge.svg)](https://scan.coverity.com/projects/2506)

Catcierge
=========
![Catcierge](https://raw.githubusercontent.com/JoakimSoderberg/catcierge-examples/master/diy/small_logo.jpg)

Catcierge is an image recognition and RFID detection system for a DIY cat door.
It can be used to detect prey that the cat tries to bring in,
or if a neighbour cat is trying to get in. The design is targeted for use with the
Raspberry Pi including the camera board. However, it also runs on Windows, Linux and OSX with a normal webcam.

There is also preliminary support for reading animal RFID tags commonly used for cats
(the kind that veterinarians insert into their necks).

Background
----------
The Catcierge project came about to solve the problem of our cat having the
nasty habit of delivering "gifts" through our cat door in the form 
of dead, or partly dead / fully alive rodents or birds.

Instead of simply not allowing our cat to use the cat door like normal people
I of course set out to find a high-tech solution to solve my perdicament.

I found the [Flo Control project][flo_control] project and based the general idea
on that setup (detecting prey based on the cats head profile).

The first implementation used a simple template matching technique, but after
evaluating that for a while I realised a better solution was needed. 
Instead I trained a Haar Cascade recognizer to find the cats head, and then
used various other techniques to detect if it has prey in it's mouth or not.
This technique has turned out to be very reliable and successful with
hardly any false positives. The training data and results for the Haar cascade
training can be found in a separate repository 
[https://github.com/JoakimSoderberg/catcierge-samples](catcierge-samples)

Hardware design details
-----------------------
To read more about how to build your own hardware that this code can run on, and the development story, see the webpage: [http://joakimsoderberg.github.io/catcierge/](http://joakimsoderberg.github.io/catcierge/)

Dependencies
------------
For the image recognition catcierge uses OpenCV via the 
[raspicam_cv library][raspicam_cv] written by [Emil Valkov][emil_valkov]
(which is included in the catcierge source).

Compiling
---------
Catcierge uses the CMake build system. To compile:

### Raspberry Pi:

First, to install OpenCV on raspbian:

```bash
$ sudo apt-get install cmake opencv-dev build-essential
```

```bash
$ git clone https://github.com/JoakimSoderberg/catcierge.git
$ cd catcierge
$ git submodule update --init # For the rpi userland sources.
$ ./build_userland.sh
$ mkdir build && cd build
$ cmake ..
$ make
```

If you don't have any [RFID cat chip reader][rfid_cat] you can exclude
it from the compilation:

```bash
$ cmake -DWITH_RFID=OFF ..
```

If you already have a version of the [raspberry pi userland libraries][rpi_userland] built,
you can use that instead:

```bash
$ cmake -DRPI_USERLAND=/path/to/rpi/userland ..
```

However, note that the program only has been tested with the submodule version of
the userland sources.

### Linux / OSX

Use your favorite package system to install OpenCV.

You can also use your own build of OpenCV when compiling:

from git [https://github.com/itseez/opencv](https://github.com/itseez/opencv)

or download: [http://opencv.org/downloads.html](http://opencv.org/downloads.html)

```bash
$ git clone <url>
$ cd catcierge
$ mkdir build && cd build
$ cmake -DRPI=0 .. # Important, don't compile the Raspberry Pi specifics...
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

Download OpenCV 2.x for Windows: [http://opencv.org/downloads.html](http://opencv.org/downloads.html)

Unpack it to a known path (you need this when compiling).

Assuming you're using [git bash](http://git-scm.com/) and [Visual Studio Express](http://www.visualstudio.com/downloads/download-visual-studio-vs) (or more advanced version).

```bash
$ git clone <url>
$ cd catcierge
$ mkdir build && cd build
$ cmake -DOpenCV_DIR=/c/PATH/TO/OPENCV/build .. # The OpenCV path must contain OpenCVConfig.cmake
$ cmake --build .     # Either build from command line...
$ start catcierge.sln # Or launch Visual Studio and build from there...
```

Running the main program
------------------------
The main program is named [catcierge_grabber2](catcierge_grabber2.c) which 
performs all the logic of doing the image recognition, RFID detection and
deciding if the door should be locked or not.

For more help on all the settings:

```bash
$ ./catcierge_grabber2 --help
```

Test programs
-------------
While developing and testing I have developed a few small helper programs.
There are both prototypes written in Python, as well as test programs that
uses the C code from the real program. The Python and C versions of OpenCV
behaves slightly differently in some cases, I am not sure why exactly.

To test the image recognition there is a test program 
[catcierge_tester](catcierge_tester.c) that allows you to specify an image
to match against.

Template matcher:

```bash
$ ./catcierge_tester --matcher template --snout /path/to/image/of/catsnout.png --images *.png --show
```

Haar matcher:

```bash
$ ./catcierge_tester --matcher haar --cascade /path/to/catcierge.xml --images *.png --show
```

Likewise for the RFID matching:

```bash
$ ./catcierge_rfid_tester
```

### Prototypes

**!Note! The below prototype is quite outdated. For the Haar cascade matcher the
prototype can be found in the [catcierge-samples][catcierge_samples] repository.**

To test different matching strategies there's a Python prototype as well
in the aptly named "protoype/" directory. The prototype is named after my
cat [higgs.py](prototype/higgs.py). This was the first prototype used to
create the Template matcher technique. 

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
[rpi_userland]: https://github.com/raspberrypi/userland
[catcierge_samples]: https://github.com/JoakimSoderberg/catcierge-samples
