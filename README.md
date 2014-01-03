
Catsnatch
=========
An image recognition and RFID cat chip solution for detecting "gifts" and
neighbour cats for an automated cat door system. Designed for use with the
Raspberry Pi including camera board.

Background
----------
The Catsnatch project came about to solve the problem of our cat having the
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
To be written.

Dependencies
------------
For the image recognition catsnatch users OpenCV using the 
[raspicam_cv library][raspicam_cv] library written by [Emil Valkov][emil_valkov]
(which is included in the catsnatch source).

To install OpenCV:

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

Compiling
---------
Catsnatch uses the CMake build system.

To compile on Raspbian:

```bash
$ git clone <url>
$ cd catsnatch
$ mkdir build && cd build
$ cmake -DRPI_USERLAND=/path/to/rpi/userland ..
$ make
```

If you don't have any [RFID cat chip reader][rfid_cat]

Running
-------
The main program is named [catsnatch_grabber](catsnatch_grabber.c) which 
performs all the logic of doing the image recognition, RFID detection and
deciding if the door should be locked or not.

For more help on all the settings:

```bash
$ ./catsnatch_grabber --help
```

To test the image recognition there is a test program 
[catsnatch_tester](catsnatch_tester.c) that allows you to specify an image
to match against.

```bash
$ ./catsnatch_tester --snout_image /path/to/image/of/catsnout.png test_image.png
```

Likewise for the RFID matching:

```bash
$ ./catsnatch_rfid_tester
```

[flo_control]: http://www.quantumpicture.com/Flo_Control/flo_control.htm]
[raspicam_cv]: https://github.com/robidouille/robidouille/tree/master/raspicam_cv
[emill_valkov]: http://www.robidouille.com/
