#!/bin/sh
/opt/gcc-linaro-arm-linux-gnueabihf-4.7-2013.03-20130313_linux/bin/arm-linux-gnueabihf-gcc grecorder.c -o grecorder -pthread -I/home/liangzl/Projects/dra756/targetfs/usr/include/gstreamer-1.0 -I/home/liangzl/Projects/dra756/targetfs/usr/include/glib-2.0 -I/home/liangzl/Projects/dra756/targetfs/usr/lib/glib-2.0/include -L/home/liangzl/Projects/dra756/targetfs/usr/lib -Wl,--rpath-link /home/liangzl/Projects/dra756/targetfs/usr/lib -Wl,-rpath /home/liangzl/Projects/dra756/targetfs/lib -lgstaudio-1.0 -lgstvideo-1.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 
