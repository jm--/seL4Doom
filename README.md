# seL4Doom

DOOM rocks, so does seL4! The goal of this project is to make the PC game DOOM
run on the seL4 microkernel.

# License
seL4Doom is released under GPL Version 2. Please see the file COPYING for
copyright and licensing information.


# Build Instructions

## A) Canonical Build Instructions

1) Install `repo` and all prerequisites as described here http://sel4.systems/Download/

2) Install additional prerequisites:
* GRUB - boot loader (we use the tool `grub-mkrescue`)
* xorriso version 1.2.9 or later - tool for manipulating ISO 9660 images
* At least one DOOM WAD file (a main IWAD resource file). The IWAD file used
  by the shareware version of Doom is readily available on the Internet.

If you use `apt`, then for example:
```
apt-get install grub-common
apt-get install xorriso
apt-get install doom-wad-shareware
```

3) Download and build the source code (kernel, libraries, seL4Doom).
```
mkdir seL4Doom
cd seL4Doom
repo init -u https://github.com/jm--/seL4Doom.git -b manifest
repo sync
make doom_defconfig
make menuconfig
```

Using the menu:
* Go to the seL4Doom application and verify the absolute file name of your
  WAD file (default is: /usr/share/games/doom/doom1.wad)
* Go to the kernel "Boot options" and set the graphics resolution you want
  to use; make sure the color depth is set to 32bpp. If you ran
  `make doom_defconfig` (the previous step), then the default is 640x400.

```
make
```
If all goes well, then this creates a bootable ISO cd-rom image in the
./images folder. (The Doom source code generates compiler warnings.)
```
make run
```
Runs QEMU with a virtual machine booting the above created ISO image. The
virtual serial port is redirected to stdout. You should see GRUB starting
the kernel in graphics mode. Then you should see a green/blue splash screen
for a second; then DOOM. Check the output of the serial port (i.e. the output
in the terminal you typed "make run") for debug and status information.

4) Have fun playing DOOM on seL4! Please let me know if you get compiler errors
or discover something that does not work. Thanks!


## B) General Build Information
If you want to integrate sel4Doom into your existing build infrastructure, then
here is some information you may find helpful:
* You need the experimental (not the master) branch of the kernel
* You may have to increase "Malloc limit," which is config symbol
  CONFIG_LIB_SEL4_MUSLC_SYS_MORECORE_BYTES, to something like "8388608" (8 MB)
* Enable "Implementation of a simple file system using CPIO archives," i.e.
  define CONFIG_LIB_SEL4_MUSLC_SYS_CPIO_FS
* You need a boot loader that can boot the kernel in graphics mode


# Features
* sel4Doom includes an **easter egg** in the form of two new cheat codes: `sel4`
  and `psu`. (Cheat codes are activated by typing in certain key sequences
  during game play.)
* You can boot into a command line **console** when you press `c` while the
  green/blue splash screen is displayed. The output of the console is written
  to libplatsupport's "X86 console device" (default is COM1). While the
  output is written to the serial port, the input is currently only from the
  keyboard and not the serial port. (The assumption is that one most likely runs
  the game via `qemu -serial stdio` anyway, so there is not much of a
  difference.) The console features a `ls` command, which
  displays the current content of the cpio archive included in the application.
  Type `doom` to boot into sel4Doom; type `doom -warp 1 3 -skill4` to start the
  game in episode 1, level 3, and difficulty level 4 (ultra-violence); type
  `doom -file myfile.wad` to load the WAD file myfile.wad, etc.
* Works with recalcitrant PS/2 **keyboards** (buggy "Legacy USB support" BIOS?)
  that refuse to operate in scan code set 2. Depending on the scan code of the
  first key pressed, seL4Doom uses either scan code set 2 (like libplatsupport)
  or scan code set 1.


# TODOs
I've added some project ideas to the issue tracker system:
https://github.com/jm--/seL4Doom/issues
You are hereby invited to help bring DOOM to seL4! It's a lot of fun!
