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
-     `librealsense`.  This is shipped in the old 2.38 version in Fedora 33 but
      it works for me.
-     gtkmm 3.

For both `librealsense` and gtkmm it is of course necessary to install the `-devel`
sub-package as well.


Testing
-------

Before running OBS with the plugin, when all other things can potentially go wrong,
use the `testrealsense` binary.  It consists of just a few lines of C++ using
`gtkmm` to create an appropriately sized window and display the frames the camera
provides.  Nothing else.  The progam is not build and shipped when you use the
include RPM `.spec` file.


Using the plugin with OBS
-------------------------

After installation the plugin should be immediately found by OBS.  Adding it as
a source happens just as for other cameras, just select the "RealSense Greenscreen"
source.

The property dialog allows to set the maximum distance (in meters) and greenscreen
color.

After the camera source has been added one can use the chroma key filter.  To enable
the filter select the `RealSense Greenscreen` source in the `Sources` list.  Right
click on the entry to bring up the context dialog and select the `Filters` menu item.
This allows to add the `Chroma Key` effect filter.  Just make sure to select the
key color as selected for the source and play a bit with the `Similarity` and
`Key Color Spull Reduction` sliders to get an acceptable result.



Caveats
-------

This plugin has so far been tested only on my machine:

-    The only camera tested so far is the L515.  I hope that the `librealsense2`
     library handles the other libraries the same.
-    I have not even tested what happens no camera is attached.
-    it definitely is no tested what happens if multiple RealSense
     cameras are attached.  The `librealsense2` documentation specifies
     the first found device is used.
-    The resolution used is also the default one selected by the
     library.  It might be possible to overwrite this selection.
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


Author
------
Ulrich Drepper <drepper@gmail.com>

Parts of the realsense-greenscreen.cc are derived from the rs-align-advanced
example in Intel's `librealsense` which has the copyright notice

     // Copyright(c) 2017 Intel Corporation. All Rights Reserved.
     // License: Apache 2.0. See LICENSE file in root directory.
