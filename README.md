# hp9816emu an HP9816 Emulator for Linux

This is an emulator for the HP9816 desktop workstation that was based on an 8MHz Motorola 68000 processor.

The code was adapted from Olivier de Smet's 98x6 emulator for windows
 [Website](https://sites.google.com/site/olivier2smet2/hp_projects/hp98x6)

The UI graphics is implemented using Maorong Zou's [EZWGL](https://github.com/dpenkler/EZWGL) library

License: GPL V3

## Installation


## Features

The HPIB peripherals are preconfigured on select code 7

Bus address 0: hp9121 amigo floppy disc drive units 0 (H700) and 1 (H701)
               This is mainly for the HPL2 operating system.
	       HPL2 boots from it but then hangs. (Workaround: boot HPL from ss/80 drive)
	       Basic and Pascal OK.

Bus address 1: printer - prints to file on host local file system:
               printer-xxx.txt

Bus address 2: hp9122 ss/80 floppy disc drive units 0 (H720) and 1 (H721)

Bus address 3: hp7908 cs/80 hard drive (H730)

Bus address 4: hp7908 cs/80 hard drive (H740)

When a H7XX label is clicked a menu to load the corresponding disc image is posted.

## Files

### A number of files are required for the emulator to work:

 9816FontW.ppm      - the characterset fonts in white
 
 9816A.ppm          - background bitmap for labels and annunciators
 
 9816-L.KML         - configuration file for Rom, buttons and annunciators
 
 rom30.bin          - the binary contents of Rom3.0 (9816 BIOS)

### Other files:

  Disk Image Files

When the emulator is RUN initially it executes the boot rom which will
search for an operating system to boot from the discs. To associate
a disk image with a drive click on the drive icon H7xx corresponding
to the drive model you have an image for and load the
appropriate disk image. Disk image files have the .hpi suffix and can
be created using Ansgar Kuekes' [hpdir](https://hp9845.net/9845/projects/hpdir/) utility.

  System Image Files

It is possible to save the current running system in a system image file.

If a system image file is passed as a runtime argument or loaded via the System menu the emulator loads and runs the image.
Note: All files in use by the system when the image was created must still be present (kml and hpi files).

## Keyboard

   The HP9816 special keys are mapped to the US QWERTY keyboad as follows:
   ```
   UK Keyboard       HP9816 Key
   -------------     ----------
   [Escape]        = Clear I/O
   [Shft]+[Escape] = STOP
   \               = Execute
   |               = Shift Execute
   [Enter]         = Enter
   [Alt]+[Enter]   = Execute
   [Alt]+ ~        = Step
   [Home]          = Clear to end
   [Alt]+[Home]    = Recall
   [Up]            = Up
   [Alt]+[Up]      = Alpha 
   [Down]          = Down
   [Alt]+[Down]    = Graphics
   [Left]          = Left
   [Alt]+[Left]    = Pause
   [Right]         = Right
   [Alt]+[Right]   = Continue
   [Pg Up]         = Shift up
   [Alt]+[PG Up]   = Result
   [Pg Dn]         = Shift down
   [Alt]+[Pg Dn]   = Edit
   [Ins]           = Insert char
   [Alt]+[Ins]     = Insert line
   [Del]           = Delete char
   [Alt]+[Del]     = Delte line
```

For HPL }  = Right arrow symbol (assignment)

The mappings are defined in keyboard.c


## Resources

For history, documentation and software for the 9816 see the wonderful [HP Comupter Museum](http://www.hpmuseum.net/display_item.php?hw=4)
maintained by the wizards of Oz.

## Issues and enhancement requests

Use the [Issue](https://github.com/dpenkler/hp9816emu/issues) feature in github to post requests for enhancements or bugfixes.
