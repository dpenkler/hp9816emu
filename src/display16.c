/*
 *   Display16.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
// Display driver.
// Update is done in real-time at each acces in video-memory in the graph or alpha bitmap.
// CRT refresh frequency is emulated with a timer based on 68000 cycle
// will create screen bitmap and copy it to main display
// counter on real-speed and with High performance counter otherwise at 1/60 sec.
//
// 
// HP9816  with 09816-66582 enhancement
// basic graphic 400x300 at zoom 2,2		16 Kb 
// basic alpha 800x600 no zoom,  80x25	2kb with effect (blink not yet done)
//
//

#include "common.h"
#include "hp9816emu.h"
#include "kml.h"
#include "mops.h"

#define DEBUG_DISPLAY16G

#if defined DEBUG_DISPLAY16G
static TCHAR buffer[256];
static int k;
#define OutputDebugString(X) fprintf(stderr,X)
#endif

#define ALPHASIZE 4096

//
// Display for writing byte a at video memory address d
// for alpha part dual ported ram address with s effect byte
//
#define CD Chipset.Display16

static VOID WriteToMainDisplay16(BYTE a, WORD d, BYTE s) {
  INT  x0, xmin, xmax;
  INT  y0, ymin, ymax;
  int	op;
  WORD	dy;
  WORD	delta;			// distance from display start in byte

  d &= 0x07FF;

  if (d >= (CD.start & 0x07FF)) {
    delta = d - (CD.start & 0x07FF);
  } else {
    delta = d + (0x0800 - (CD.start & 0x07FF));
  }
  d = CD.start + delta;					// where to write
  d &= 0x3FFF;
  if (delta < 2000) {					// visible 80x25 ?
    y0 = (delta / 80) * CD.alpha_char_h + scrY;	        // where on Y
    x0 = (delta % 80) * CD.alpha_char_w + scrX;	        // where on X
    xmin = xmax = x0;					// min box
    ymin = ymax = y0;
    dy = 0;
    switch (d >> 12) { // get TEXT (bit13), KEYS (bit12)
    case 0: op = GXclear;	                  break; // nothing displayed
    case 1: op = (d & 0x0800) ? GXcopy : GXclear; break; // keys on, text off
    case 2: op = (d & 0x0800) ? GXclear : GXcopy; break; // keys off, text on
    default: op = GXcopy;		          break; // keys on, text on
    }
    // change line in font bitmap
    if (s & 0x04) dy += 4*CD.alpha_char_h;	// underline
    if (s & 0x08) dy += 2*CD.alpha_char_h;	// half bright
    if (s & 0x01) dy += CD.alpha_char_h;	// inverse video
    // copy the font bitmap part to alpha1 bitmap
    emuBitBlt(hAlpha1BM, x0, y0, CD.alpha_char_w, CD.alpha_char_h,
	      hFontBM, (int)(a) * CD.alpha_char_w, dy, op);
    xmax += CD.alpha_char_w;	// adjust max box
    ymax += CD.alpha_char_h;
    // adjust rectangle
    if (xmax > CD.a_xmax) CD.a_xmax = xmax;
    if (xmin < CD.a_xmin) CD.a_xmin = xmin;
    if (ymax > CD.a_ymax) CD.a_ymax = ymax;
    if (ymin < CD.a_ymin) CD.a_ymin = ymin;
  } else {
    // fprintf(stderr,"%06X: Write Main Display %02X - %04X : %04X : %04X\n",
    //         Chipset.Cpu.PC, a, CD.start, d, CD.end);
  } 
}

//
// remove the blinking cursor if last on directly on main display bitmap
//
static VOID Cursor_Remove16(VOID) {
  WORD d;
  INT x0, y0;

  if (CD.cursor_last) {				// was on
    d = CD.cursor;
    if (d < CD.start) d += 0x0800;
    if ((d >= CD.start) && (d < CD.end)) {	// visible 
      if (d & 0x2000) {// alpha on
	y0 = ((d - CD.start) / 80) * CD.alpha_char_h + scrY;	// where on Y
	x0 = ((d - CD.start) % 80) * CD.alpha_char_w + scrX;	// where on X
	emuBitBlt(hWnd, x0, y0, CD.alpha_char_w, CD.alpha_char_h, 
		  hAlpha1BM, x0, y0, GXcopy);
	if (CD.graph_on)			// put graph part again if it was on
	  emuPutImage(hWnd, x0, y0, CD.alpha_char_w, CD.alpha_char_h, hGraphImg, GXxor);
      }
    }
    CD.cursor_last = 0;				// cursor is off
  }
}

//
// display the blinking cursor if needed
//
static VOID Cursor_Display16(VOID) {
  int d;
  int x0, y0;
  int op;

  CD.cursor = CD.cursor_new;			// new location on screen	
  if (CD.cursor_blink & CD.cnt)	return;		// should be off, return
  if (CD.cursor_b < CD.cursor_t) return;	// no size, return

  d = CD.cursor;
  if (d < CD.start) d += 0x0800;
  if ((d >= CD.start) && (d < CD.end)) {        // visible ?
    if (d & 0x2000) {// alpha on
      y0 = ((d - CD.start) / 80) * CD.alpha_char_h + scrY + CD.cursor_t;
      x0 = ((d - CD.start) % 80) * CD.alpha_char_w + scrX; 
      emuPatBlt(hWnd, x0, y0, CD.alpha_char_w, CD.cursor_h, GXset);
      if (CD.graph_on)
	emuPutImage(hWnd, x0, y0, CD.alpha_char_w, CD.cursor_h, hGraphImg, GXxor);
    }
  }
  CD.cursor_last = 1;			        // cursor is on
}

//################
//#
//#    Public functions
//#
//################

//
// write in display IO space
// 
// 6845 default regs for 9816 (80x25)
// R00 : 114
// R01 :  80
// R02 :  76
// R03 :   7
// R04 :  26
// R05 :  10
// R06 :  25
// R07 :  25
// R08 :   0
// R09 :  14
// R10 :  76	cursor shape : start at 14d, blink a 1/32
// R11 :  13	end at 19d
// 
// R11 :  $0D
// R12 :  $30 display pos : $30D0 : both area displayed (3) with 2 lines of soft keys (D0)
// R13 :  $D0  ""
// R14 :  $30 cursor pos : $30CF
// R15 :  $CF  ""

BYTE Write_Display16(BYTE *a, WORD d, BYTE s) {
  WORD	start_new;

  d &= 0x3FFF;

  if ((d >= 0x2000) && (d <= 0x2000+(ALPHASIZE<<1)-s)) {
    d -= 0x2000;						// base at 0
    while (d >= (WORD) (ALPHASIZE)) d -= (WORD) (ALPHASIZE);	// wrap address
    //		if ((s == 1) && !(d & 0x0001))
    //			return BUS_ERROR;
    // no blink and all
    while (s) {
      CD.alpha[d] = *(--a); 
      
      //if (*a > 32 && *a < 255) fprintf(stderr," Main Display %04X %c : %02X (%02X)\n", d, *a , *a, s);
      
      s--;
      if ((d & 0x0001) == 0x0001)  WriteToMainDisplay16(*a, d >> 1, CD.alpha[d-1]);
      d++;
      // else return BUS_ERROR;
    }
    return BUS_OK;
  }
  if (s == 2) {						// acces in word
    if (d == 0x0000) {					// 6845 reg
      --a;
      CD.reg = *(--a);
      //fprintf(stderr,"%06X: DISPLAY : set reg to %02d\n",Chipset.Cpu.PC, Chipset.Display16.reg);
      return BUS_OK;
    }
    else if (d == 0x0002) {				// 6845 regs
      --a;
      CD.regs[CD.reg] = *(--a);
      switch (CD.reg) {
      case 10:
	CD.cursor_t = (((*a) & 0x1F) * 23) / 13;
	switch (((*a) & 0x60) >> 5) {
	case 0:  CD.cursor_blink = 0x00; break;	//always on
	case 1:  CD.cursor_blink = 0xFF; break;	// off
	case 2:  CD.cursor_blink = 0x20; break;	// blink 1/16
	default: CD.cursor_blink = 0x10;	// blink 1/32
	}
	if (CD.cursor_b >= CD.cursor_t)  CD.cursor_h = CD.cursor_b - CD.cursor_t + 1;
	break;
      case 11:
	CD.cursor_b = (((*a) & 0x3F) * 23) / 13;
	if (CD.cursor_b >= CD.cursor_t)  CD.cursor_h = CD.cursor_b - CD.cursor_t + 1;
	break;
      case 12:
      case 13:
	start_new = ((CD.regs[12] << 8) + CD.regs[13]) & 0x3FFF;

	if (start_new != CD.start) {
	  fprintf(stderr,"newstart %04x, was %04x\n",start_new,CD.start);
	  CD.start = start_new;
	  CD.end = (start_new + 80*25) & 0x3FFF;
	  UpdateMainDisplay16(FALSE);		// change of start
	}
	break;
      case 14:
      case 15:
	CD.cursor_new = ((CD.regs[14] << 8) + CD.regs[15]) & 0x3FFF;
	break;
      default:
	break;
      }
      
      //fprintf(stderr,"%06X: DISPLAY : Write.w reg %02d = %02X\n", Chipset.Cpu.PC, Chipset.Display16.reg, *a);

      return BUS_OK;
    } 
  } else if (s == 1) {			 	        // acces in byte
    if (d == 0x0001) {					// 6845 regs
      CD.reg = *(--a);
      //fprintf(stderr,"%06X: DISPLAY : set reg to %02d\n",Chipset.Cpu.PC, Chipset.Display16.reg);
      return BUS_OK;
    }
    else if (d == 0x0003) {				// 6845 regs
      CD.regs[CD.reg] = *(--a);
      switch (CD.reg) {
      case 10:
	CD.cursor_t = (((*a) & 0x1F) * 23) / 13;
	switch (((*a) & 0x60) >> 5) {
	case 0:  CD.cursor_blink = 0x00; break;	//always on
	case 1:  CD.cursor_blink = 0xFF; break;	// off
	case 2:  CD.cursor_blink = 0x20; break;	// blink 1/16
	default: CD.cursor_blink = 0x10;	// blink 1/32
	}
	if (CD.cursor_b >= CD.cursor_t) CD.cursor_h = CD.cursor_b - CD.cursor_t + 1;
	break;
      case 11:
	CD.cursor_b = (((*a) & 0x3F) * 23) / 13;
	if (CD.cursor_b >= CD.cursor_t) CD.cursor_h = CD.cursor_b - CD.cursor_t + 1;
	break;
      case 12:
      case 13:
	start_new = ((CD.regs[12] << 8) + CD.regs[13]) & 0x3FFF;
	
	if (start_new != CD.start) {
	  CD.start = start_new;
	  CD.end = (start_new + 80*25) & 0x3FFF;
	  UpdateMainDisplay16(FALSE);		// change of start
	}
	break;
      case 14:
      case 15:
	CD.cursor_new = ((CD.regs[14] << 8) + CD.regs[15]) & 0x3FFF;
	break;
      default:
	break;
      }

      //fprintf(stderr,"%06X: DISPLAY : Write.w reg %02d = %02X\n", Chipset.Cpu.PC, Chipset.Display16.reg, *a);

      return BUS_OK;
    }
  }
  return BUS_ERROR;
}

//
// read in display IO space (too wide alpha ram, but nobody cares ...)
// no CRT configuration reg for 9816 and 9836A
//
BYTE Read_Display16(BYTE *a, WORD d, BYTE s) {
  d &= 0x3FFF;
  if ((d >= 0x2000) && (d <= 0x2000 + (ALPHASIZE<<1)-s)) {// DUAL port alpha video ram

    // fprintf(stderr,"%06X: DISPLAY : Read alpha %04x = %02X\n", Chipset.Cpu.PC, d, CD.alpha[d & 0x3FF]);

    d -= 0x2000;
    while (d >= (WORD) (ALPHASIZE)) d -= (WORD) (ALPHASIZE);
    while (s) {
      *(--a) = CD.alpha[d++]; 
      s--;
    }
    return BUS_OK;		
  }
  if ((d == 0x0003) && (s == 1)) {	// 6845 regs
    *(--a) = CD.regs[CD.reg];

    //fprintf(stderr,"%06X: DISPLAY : Read reg %02d = %02X\n",
    //Chipset.Cpu.PC, CD.reg, CD.regs[CD.reg]);

    return BUS_OK;
  }
  return BUS_ERROR;
}

//
// read in graph mem space 
//
BYTE Read_Graph16(BYTE *a, WORD d, BYTE s) {
  BYTE graph_on;

  graph_on = (d & 0x8000) ? 0 : 1;	// address of read access switches graphics ON/OFF
  if (graph_on && ! CD.graph_on) {	// was off and now on -> should redisplay all graph
    CD.g_xmax = CD.graph_width;
    CD.g_xmin = 0;
    CD.g_ymax = CD.graph_height;
    CD.g_ymin = 0;
  }
  if (!graph_on && CD.graph_on) {	// was on and now off -> should redisplay all alpha
    CD.a_xmax = CD.alpha_width;
    CD.a_xmin = 0;
    CD.a_ymax = CD.alpha_height;
    CD.a_ymin = 0;
  }
  if (graph_on ^ CD.graph_on) {		// changed 
#if defined DEBUG_DISPLAY16G
    k = wsprintf(buffer,"%06X: DISPLAY : graph on %02d -> %02X\n",
		 Chipset.Cpu.PC, CD.graph_on, graph_on);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
  }
  CD.graph_on = graph_on;	// new graph state

  while (s) {			// do the read with endianness
    d &= 0x7FFF;
    *(--a) = CD.graph[d]; 
#if defined DEBUG_DISPLAY16GH
    if (Chipset.Cpu.PC > 0x010000) {
      k = wsprintf(buffer,"%06X: GRAPH(%d) : Read %04X = %02X\n",
		   Chipset.Cpu.PC, Chipset.Display.graph_on, d, *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
    }
#endif
    d++;
    s--;
  }
  return BUS_OK;
}
static void PutPixel(int x, int y, unsigned int pixel) {
   XPutPixel(hGraphImg, x,   y,   pixel);
   XPutPixel(hGraphImg, x+1, y,   pixel);
   XPutPixel(hGraphImg, x,   y+1, pixel);
   XPutPixel(hGraphImg, x+1, y+1, pixel);
}

BYTE Write_Graph16(BYTE *a, WORD d, BYTE s) {
  LPBYTE ad;
  WORD x0, y0;
  BYTE graph_on;
  int x,y;

  graph_on = (d & 0x8000) ? 0 : 1;	// address of read access switches gprahics ON/OFF
  if (graph_on && !CD.graph_on) {	// was off and now on -> should redisplay all graph
    CD.g_xmax = CD.graph_width;
    CD.g_xmin = 0;
    CD.g_ymax = CD.graph_height;
    CD.g_ymin = 0;
  }
  if (!graph_on && CD.graph_on) {	// was on and now off -> should redisplay all alpha
    CD.a_xmax = CD.alpha_width;
    CD.a_xmin = 0;
    CD.a_ymax = CD.alpha_height;
    CD.a_ymin = 0;
  }
  if (graph_on ^ CD.graph_on) {
#if defined DEBUG_DISPLAY16G
    k = wsprintf(buffer,_T("%06X: DISPLAY : graph on %02d -> %02X\n"),
		 Chipset.Cpu.PC, CD.graph_on, graph_on);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
  }
  CD.graph_on = graph_on;		// new graph state

  while (s) {				// do the write on mem and bitmap
    d &= 0x7FFF;
    CD.graph[d] = *(--a);		// write even for even byte for 9816... nobody cares
#if defined DEBUG_DISPLAY16GH
    if (Chipset.Cpu.PC > 0x010000) {
      k = wsprintf(buffer,_T("%06X: GRAPH(%d) : Write %04X <- %02X\n"),
		   Chipset.Cpu.PC, Chipset.Display.graph_on, d, *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
    }
#endif
    if ((d < CD.graph_visible) && (d & 0x0001)) {	// visible and odd byte only
      x0 = ((d>>1) % CD.graph_bytes)*8*2;
      y0 = ((d>>1) / CD.graph_bytes)*2;
      x = x0;
      y = y0;

      PutPixel(x, y, (*a & 0x80) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x40) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x20) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x10) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x08) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x04) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x02) ? CD.default_pixel : 0); x+=2;
      PutPixel(x, y, (*a & 0x01) ? CD.default_pixel : 0); x+=2;

      // adjust rectangle
      if (x > CD.g_xmax)   CD.g_xmax = x;
      if (x0 < CD.g_xmin)  CD.g_xmin = x0;
      if (y+2 > CD.g_ymax) CD.g_ymax = y+2;
      if (y0 < CD.g_ymin)  CD.g_ymin = y0;
      }
    d++;
    s--;
  } 
  return BUS_OK; 
}

//
// reload graph bitmap from graph mem (only used when reloading system image in files.c)
//
VOID Reload_Graph16(VOID) {
  BYTE s;
  WORD d;
  int x,y;
  
  for (d = 1; d < CD.graph_visible; d+=2) {
    s = CD.graph[d];
    x = 2*((d>>1) % CD.graph_bytes)*8;
    y = 2*((d>>1) / CD.graph_bytes);
    PutPixel(x, y, (s & 0x80) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x40) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x20) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x10) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x08) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x04) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x02) ? CD.default_pixel : 0); x+=2;
    PutPixel(x, y, (s & 0x01) ? CD.default_pixel : 0);
  }
  if (CD.graph_on) {
    CD.g_xmax = CD.graph_width;
    CD.g_xmin = 0;
    CD.g_ymax = CD.graph_height;
    CD.g_ymin = 0; 
  }
}

//
// update alpha display when start change only
// called with FALSE when start changes
// called with TRUE on image load and system reset (files.c & mops.c)
//
VOID UpdateMainDisplay16(BOOL bForce) { 
  WORD	i, n_lines;
  INT	x0;			
  INT	y0;
  WORD	dVid;
  WORD	d;
  int	op;
  WORD	dy;
  BYTE	s;

  //fprintf(stderr,"%06X: Update Main Display, %d\n", Chipset.Cpu.PC, bForce);

  d = CD.start;			// adress of begining of screen

  n_lines = 25;			// 25 lines to redraw
  dVid = ((d & 0x07FF) << 1) + 1;	// address (11 bits) in video ram (2 bytes per char)
  y0 = scrY;
  x0 = scrX;	
  n_lines *= 80;			// number of characters to update
  for (i = 0; i < n_lines; i++) {	
    s = CD.alpha[dVid-1];		// effect byte
    dy = 0;
    switch (d >> 12) {			// get TEXT (bit13), KEYS (bit12)
    case 0: op = GXclear;	                  break;    // nothing displayed
    case 1: op = (d & 0x0800) ? GXcopy : GXclear; break;    // keys on, text off
    case 2: op = (d & 0x0800) ? GXclear : GXcopy; break;    // keys off, text on
    default: op = GXcopy;				    // keys on, text on
    }
    if (s & 0x04) dy += 4*CD.alpha_char_h; 	// underline
    if (s & 0x08) dy += 2*CD.alpha_char_h;      // half bright
    if (s & 0x01) dy += CD.alpha_char_h;        // inverse video
    emuBitBlt(hAlpha1BM, x0, y0, CD.alpha_char_w, CD.alpha_char_h, 
	      hFontBM, CD.alpha[dVid] * CD.alpha_char_w, dy,
	      op);
    dVid += 2;
    dVid &= 0x0FFF;				// wrap at 4k
    x0 += CD.alpha_char_w;
    if (x0 == (scrX + 80*CD.alpha_char_w)) {
      y0 += CD.alpha_char_h;
      x0 = scrX;
    }
    d++;
    d &= 0x3FFF;				// wrap at 16k
  }
  // force a full copy at next vbl
  CD.a_xmin = 0;
  CD.a_xmax = CD.alpha_width;
  CD.a_ymin = 0;
  CD.a_ymax = CD.alpha_height;
  CD.g_xmin = CD.graph_width;
  CD.g_xmax = 0;
  CD.g_ymin = CD.graph_height;
  CD.g_ymax = 0; 
}

//
// only copy what is needed from alpha1 and graph to screen
//
VOID Refresh_Display16(BOOL bForce) {
  BYTE do_flush = 0;

  if (bForce) {			// adjust rectangles to do a full redraw
    CD.a_xmin = 0;
    CD.a_xmax = CD.alpha_width;
    CD.a_ymin = 0;
    CD.a_ymax = CD.alpha_height;
    CD.g_xmin = CD.graph_width;
    CD.g_xmax = 0;
    CD.g_ymin = CD.graph_height;
    CD.g_ymax = 0;
  }
  if (CD.a_xmax > CD.a_xmin) {	// something to draw on alpha
    emuBitBlt(hWnd, CD.a_xmin, CD.a_ymin, CD.a_xmax-CD.a_xmin, CD.a_ymax-CD.a_ymin, 
	      hAlpha1BM, CD.a_xmin, CD.a_ymin, 
	      GXcopy);
    if (CD.graph_on) { // add graph corresponding graphics area
      emuPutImage(hWnd,CD.a_xmin, CD.a_ymin, CD.a_xmax-CD.a_xmin, CD.a_ymax-CD.a_ymin, hGraphImg, GXxor);
    }
    do_flush = 1;
    CD.a_xmin = CD.alpha_width;		// nothing more to re draw at next vbl
    CD.a_xmax = 0;
    CD.a_ymin = CD.alpha_height;
    CD.a_ymax = 0;
  }
  if ((CD.g_xmax > CD.g_xmin) && CD.graph_on) {	// something to draw on graphics
    emuBitBlt(hWnd, CD.g_xmin, CD.g_ymin, // draw the alpha part overlapping graph area
	      CD.g_xmax-CD.g_xmin,  CD.g_ymax-CD.g_ymin, 
	      hAlpha1BM, CD.g_xmin, CD.g_ymin, 
	      GXcopy);
    emuPutImage(hWnd,CD.g_xmin, CD.g_ymin, CD.g_xmax-CD.g_xmin, CD.g_ymax-CD.g_ymin, hGraphImg, GXxor);
    do_flush = 1;
    CD.g_xmin = CD.graph_width;		// nothing more to re draw at next vbl
    CD.g_xmax = 0;
    CD.g_ymin = CD.graph_height;
    CD.g_ymax = 0;
  }
  if (do_flush) {
    emuFlush();
  }
}

//
// do VBL
//

VOID do_display_timers16(VOID)	{
  CD.cycles += Chipset.dcycles;
  if (CD.cycles >= dwVsyncRef) {		// one vsync
    CD.cycles -= dwVsyncRef;
    CD.cnt ++;
    CD.cnt &= 0x3F;
    Refresh_Display16(FALSE);	// just rects
    UpdateAnnunciators(FALSE);	// changed annunciators only
    Cursor_Remove16();		// do the blink 
    Cursor_Display16();
  }
}