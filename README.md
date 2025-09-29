# gtkmm4-database-viewer-sample-test
the program is working but is still under development

Building on Windows:
====================

1.) Install MSYS2
-----------------
Download and run the msys2 installer from http://msys2.github.io/ Tested only with the 64bit version. Make sure that the path you select for installation doesn’t contain any spaces.

2.) Start MSYS console
----------------------
Launch the Start Menu item “MSYS2 mingw 64 bit” you should be greeted with a console window. All steps below refer to what you should type into that window.

3.) Install updates
-------------------
Type:

   pacman -Syu

if it tells you to close restart msys, close the console window and start it again. Then run pacman -Syu again.

4.) Install dependencies
------------------------
Type/paste

   pacman -S \\ \
   mingw-w64-x86_64-gcc \\ \
   mingw-w64-x86_64-pkgconf \\ \
   mingw-w64-x86_64-gtkmm4 \\ \
   zip \\ \
   unzip \\ \
   git \\ \
   mingw-w64-x86_64-libgda \\ \
   --needed

When prompted, just hit return. Sit back and wait for it to install what’s almost a complete linux environment.

Before continuing you may change to another directory. It easiest to type cd followed by a space and drop the folder you want to change to on the window.

5.) Clone gtkmm4-database-viewer-sample-test by type/paste on commandline:
---------------------------------------------------------------------

   git clone https://github.com/Peta-T/gtkmm4-database-viewer-sample-test \
   cd gtkmm4-database-viewer-sample-test

6.) Build it - type on command line:
------------------------------------

  g++ -std=c++20 main.cc `pkg-config --cflags --libs gtkmm-4.0 libgda-6.0 ` -o app -DUSE_LIBGDA6

7.) Run app - type on command line:
-----------------------------------

   ./app


