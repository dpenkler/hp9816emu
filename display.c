/*
 *   Display.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
// Common part of display drivers, create bitmaps and all
//

#include "common.h"
#include "hp9816emu.h"
#include "kml.h"
#include "mops.h"

Pixmap	hFontBM;
Pixmap	hAlpha1BM;
Pixmap	hAlpha2BM;
XImage *hGraphImg;
Pixmap  hMainBM;
Pixmap  hScreenBM;

LPBYTE	pbyAlpha1;
LPBYTE	pbyAlpha2;
LPBYTE	pbyGraph;
LPBYTE  pbyScreen;

UINT    bgX = 0;		// offset of background X (from KML script)
UINT    bgY = 0;		// offset of background Y (from KML script)
UINT    bgW = 0;		// width of background (from KML script)
UINT    bgH = 0;		// height of background (from KML script)
UINT    scrX = 0;		// offset of screen X (from KML script)
UINT    scrY = 0;		// offset of screen Y (from KML script)

#define CD16 Chipset.Display16

//################
//#
//#    Low level subroutines
//#
//################


//################
//#
//#    Public functions
//#
//################

//
// allocate screen bitmaps and font and find mother board switches ...
// (I know, strange place to do that) 
//
VOID CreateScreenBitmap(VOID)  {
//	BYTE i;

// HP9816
//
// SW1		/---------------------- CRT Hz setting
//              |/--------------------- HP-IB System controller
//              ||/-------------------- Continuous test
//		|||/------------------- Remote Keyboard status
//              ||||/--\--------------- Baud rate
//		76543210
// default      11100101 0xE5 
// SWITCH1 0xE5
//
// SW2		/\--------------------- Character length
//              ||/-------------------- Stop bits
//              |||/------------------- Parity Enable
//		||||/\----------------- Parity
//              ||||||/\--------------- Handshake
//		76543210
// default      11000000 0xC0
// SWITCH2 0xC0
//

  Chipset.type = nCurrentRomType;	// not already assigned, do it

  Chipset.switch1 = 0xE5;
  Chipset.switch2 = 0xC0;
  CD16.alpha_width  = 800;	// width in pc pixels
  CD16.alpha_height = 600;	// height in pc pixels
  CD16.alpha_char_w =  10;
  CD16.alpha_char_h =  24;
  CD16.graph_width  = 800;	// width in pc pixels
  CD16.graph_bytes  =  50;	// 9816 width in bytes 50*8=400
  CD16.graph_height = 600;	// height in pc pixesl
  CD16.graph_visible = 30000;	// last address byte visible : 400x300/8 x 2 (only odd bytes)
  CD16.default_pixel = 0xffffff;
  // create alpha1 bitmap
  hAlpha1BM = emuCreateBitMap(CD16.alpha_width, CD16.alpha_height);
  // create alpha2 bitmap
  hAlpha2BM = emuCreateBitMap(CD16.alpha_width, CD16.alpha_height);
  // create graph image
  hGraphImg = emuCreateImage(CD16.graph_width, CD16.graph_height);
  fprintf(stderr,"Graph im bpl %d\n",hGraphImg->bytes_per_line);
  // create screen bitmap
  hScreenBM = emuCreateBitMap(CD16.alpha_width, CD16.alpha_height);
  // font bitmap
  hFontBM = emuLoadBitMap("9816FontW.ppm");	// Load bitmap font
		
  CD16.a_xmin = 0;
  CD16.a_xmax = CD16.alpha_width;
  CD16.a_ymin = 0;
  CD16.a_ymax = CD16.alpha_height;
  CD16.g_xmin = CD16.graph_width;
  CD16.g_xmax = 0;
  CD16.g_ymin = CD16.graph_height;
  CD16.g_ymax = 0;
		
}
	
	//
	// free screen bitmap and font
//
VOID DestroyScreenBitmap(VOID) {

  if (hGraphImg != NULL) {
    emuFreeImage(hGraphImg);
    hGraphImg = NULL;
  }
  if (hAlpha1BM != 0) {
    emuFreeBitMap(hAlpha1BM);
    hAlpha1BM = 0;
  }
  if (hAlpha2BM != 0) {
    emuFreeBitMap(hAlpha2BM);
    hAlpha2BM = 0;
  }
  if (hFontBM != 0) {
    emuFreeBitMap(hFontBM);
    hFontBM = 0;
  }
  if (hScreenBM != 0) {
    emuFreeBitMap(hScreenBM);
    hScreenBM = 0;
  }
}

//
// allocate main bitmap for border infos
//
BOOL CreateMainBitmap(LPCTSTR szFilename) {
  
  hMainBM = emuLoadBitMap(szFilename);
  if (hMainBM == 0) {
    return FALSE;
  }
  return TRUE;
}

//
// free main bitmap
//
VOID DestroyMainBitmap(VOID) {
  if (hMainBM != 0) {
    hMainBM = 0;
  }
}

//
// update the main window bitmap (border & all)
//
VOID UpdateMainDisplay(BOOL bForce) {
  UpdateMainDisplay16(bForce);
}

//
// refresh only the display part (with rectangle)
//
VOID Refresh_Display(BOOL bForce) {
  Refresh_Display16(bForce);
}

// 
// reload graph bitmap from graph mem (9837) 
//
VOID Reload_Graph(VOID) {
  Reload_Graph16();
}

//
// update display annuciators (via kml.c)
//
VOID UpdateAnnunciators(BOOL bForce) {
  int i,change=0;
  DWORD j=1;

  if (bForce) Chipset.pannun = ~Chipset.annun;   // force redraw all annun ?
  for (i = 1; i < 26; ++i) {
    if ((Chipset.annun ^ Chipset.pannun) & j) { // only if state changed
      DrawAnnunciator(i, (Chipset.annun & j));
      change++;
    }
    j <<= 1;
  }
  Chipset.pannun = Chipset.annun;
  if (change) emuFlush();
}

