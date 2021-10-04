OBS RealSense Virtual Greenscreen
=================================

This OBS plugin allows to use the depth sensor of the Intel RealSense USB cameras to
generate a virtual greenscreen.  The camera can already be used OBS directly as a
normal camera (via V4L2) but this does not use the depth sensor.

With this plugin the camera can be used directly.  The actual access to the camera
and the alignment of the video with the depth field is done by the `librealsense2`
library.  This plugin just performs the masking and it implements the interface to
OBS.


Building
--------

If the required packages are installed, just run `make`.  The include RPM `.spec`
file codifies the dependencies which are
-     gcc 10 or higher (tested with C++20 only)
-     `librealsense`.  Fedora 33 ships the old 2.38 version but it works for me.
-     gtkmm 3.

For both `librealsense` and gtkmm it is of course necessary to install the `-devel`
sub-package as well.


### Alternative `librealsense`

To test a `librealsense` other than the officially installed version use the
`make` command with the variable `ALT_LIBREALSENSE` pointing to the appropriate
directory:

    make ALT_LIBREALSENSE=/home/drepper/librealsense

This works with the official `librealsense` sources off of github.


Testing
-------

Before running OBS with the plugin, when all other things can potentially go wrong,
use the `testrealsense` binary.  It consists of just a few lines of C++ using
`gtkmm` to create an appropriately sized window and display the frames the camera
provides.  Nothing else.  Press the escape key to exit the program.  The progam
is not build and shipped when you use the include RPM `.spec` file.


Using the plugin with OBS
-------------------------

After installation the plugin should be immediately found by OBS.  Adding it as a source happens
just as for other cameras, just select the "RealSense Greenscreen" source.

The property dialog allows to select the device, change the resolution, set the
maximum distance (in meters) and greenscreen color.

After the camera source has been added one can use the chroma key filter.  To enable
the filter select the `RealSense Greenscreen` source in the `Sources` list.  Right
click on the entry to bring up the context dialog and select the `Filters` menu item.
This allows to add the `Chroma Key` effect filter.  Just make sure to select the
key color as selected for the source and play a bit with the `Similarity` and
`Key Color Spill Reduction` sliders to get an acceptable result.



Caveats
-------

This plugin has so far been tested only on my machine:

-    The only camera tested so far is the L515.  I hope that the `librealsense2`
     library handles the other devices the same.
-    I have not even tested what happens if no camera is attached.
-    The frequency of the picture provided is fixed at 30Hz.  Not sure
     whether this needs to be variable.
-    The depth sensor is quite noisy.  When running the `testrealsense`
     test program one can clearly see artifacts.  It might be good to
     add more filtering.  I found, though, with some tinkering with
     the settings of the plugin inside OBS it is possible to minimize
     the visible effects.
-    The code allows to set a minimum distance for the mask as well.  This
     setting is so far not carried through to the OBS properties dialog.
     Not sure this is necessary.
-    Obviously, the code has only been tested under Fedora.


Trouble Shooting
----------------

Connecting to the camera sometimes fails.  One will see then errors
from `librealsense` like

    RealSense error calling rs2_pipeline_wait_for_frames(pipe:0x1ffabe0):
        Frame didn't arrive within 15000

For me this does not mean a problem in the code.  Instead, it seems
related to the USB device connection.  Unplugging the camera and then
reattaching it usually fixes this for me.

In future I might try to automatically handle this.  Linux allows to reset
an USB device through an `ioctl` call with `USBDEVFS_RESET`.  I will give
this a try the next time I run into the problem.


If the camera is already in use the plugin will not report any resources
to OBS.  Any already source in a scene will just be zero-sized.


Author
------
Ulrich Drepper <drepper@gmail.com>

Parts of the realsense-greenscreen.cc are derived from the rs-align-advanced
example in Intel's `librealsense` which has the copyright notice

     // Copyright(c) 2017 Intel Corporation. All Rights Reserved.
     // License: Apache 2.0. See LICENSE file in root directory.
