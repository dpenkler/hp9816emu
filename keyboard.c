/*
 *   Keyboard.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//  nimitz keyboard emulation
//
// us keycodes only	
//

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"

#define CK  Chipset.Keyboard
//#define DEBUG_KEYBOARDH
//#define DEBUG_KEYBOARD
//#define DEBUG_KEYBOARDC
#if defined(DEBUG_KEYBOARD) || defined(DEBUG_KEYBOARDH) || defined(DEBUG_KEYBOARDC)
static TCHAR buffer[256];
static int k;
//	unsigned long l;
#define OutputDebugString(X) fprintf(stderr,X)
#endif


//status   76543210
//         |||||||\----------	service need (interrupt level one done)
//         ||||||\-----------	8041 busy
//         |||||\------------	cause of NMI : 0:RESET key, 1:timeout
//         ||||\-------------	??
//         |||\-------------- b7=1	/ctrl state			0,5,6:	not used
//         ||\--------------- b7=1	/shift state			1:	10msec periodic system interrupt (PSI)	
//         |\---------------- b7=1	knob count in data		2:	interrupt from one of the special timers ?
//         \-----------------		keycode in data			3:	both PSI and special timers interrupt
//									4:	the data contain a byte request by the 68000
//								        7:	power-up reset and self test done
//									8:	data contain keycode (shift & ctrl)
//									9:	data contain keycode (ctrl)
//									10:	data contain keycode (shift)
//									11:	data contain keycode
//									12:	data contain knob count (shift & ctrl)
//									13:	data contain knob count (ctrl)
//									14:	data contain knob count (shift)
//									15:	data contain knob count
//								
// 8041 registers
//	0, 1		scratch and pointer registers
//	2			flags
//	3			scratch
//	4			flags
//	5			flags
//	6			fifo
//	7			keycode of current key or 0
//	8, f		stack space
//	10			reset debounce counter
//	11			configuration jumper code
//	12			language jumper code
//	13, 17		timer outmput buffer (lsb is 13)
//	18, 19		scratch and pointer
//	1a			scratch
//	1b			timer status bits
//	1c			current knob pulse count (non hpil)
//	1d			pointer register to put data sent by 68000
//	1e			pointer register for data to send to 68000
//	1f			six counter for 804x timer interrupt
//	20			auto repeat delay
//	21			auto repeat timer
//	22			auto repeat rate
//	23			beep frequency (8041)
//	24			beep timer (counts up to zero)
//	25			knob timer (non hpil)
//	26			knob interrupt rate (non hpil)
//	27			if bit 6 is 1 the 804x timer has interrupted
//	28, 2c		not used
//	2d, 2f		time of day (3 bytes)
//	30, 31		days integer (2 bytes)
//	32, 33		fast-handshake timer (2 bytes)
//	34, 36		real time to match (3 bytes)
//	37, 39		delay timer (3 bytes)
//	3a, 3c		Cycle timer (3 bytes)
//	3d, 3f		Cycle timer save (3 bytes)
//
// language jumper code $11		0 : US normal		1 : French			2 : German
//								3 : Swedish/Finish	4 : Spanish			5 : Katakana
//								6 : 
// configuration jumper $12		bit 0 : 0 98203B keyboard, 1 98203A keyboard
//								bit 6 : 0 old code, 1 new revisions
//


// us keymap
// normal shift altgr alt shif-alt if needed

static BYTE uskeycode[129][5] = {
{   0,  0,  0,  0,  0},  //   0  nil
{   0,  0,  0,  0,  0},  //   1  nil
{   0,  0,  0,  0,  0},  //   2  nil
{   0,  0,  0,  0,  0},  //   3  nil
{   0,  0,  0,  0,  0},  //   4  nil
{   0,  0,  0,  0,  0},  //   5  nil
{   0,  0,  0,  0,  0},  //   6  nil
{   0,  0,  0,  0,  0},  //   7  nil
{   0,  0,  0,  0,  0},  //   8  nil
{  55,  0,  0,  0,  0},  //   9   ESCAPE	CLR I/O (STOP)
{  80,  0,  0,  0,  0},  //  10   1 !		
{  81,  0,  0,  0,  0},  //  11   2 @
{  82,  0,  0,  0,  0},  //  12   3 #
{  83,  0,  0,  0,  0},  //  13   4 $
{  84,  0,  0,  0,  0},  //  14   5 %
{  85,  0,  0,  0,  0},  //  15   6 ^
{  86,  0,  0,  0,  0},  //  16   7 &
{  87,  0,  0,  0,  0},  //  17   8 *
{  88,  0,  0,  0,  0},  //  18   9 (
{  89,  0,  0,  0,  0},  //  19   0 )
{  90,  0,  0,  0,  0},  //  20   - _
{  91,  0,  0,  0,  0},  //  21   = + 
{  46,  0,  0,  0,  0},  //  22   BACK	BACK SPACE
{  25,  0,  0,  0,  0},  //  23   TAB	TAB
{ 104,  0,  0,  0,  0},  //  24   Q
{ 105,  0,  0,  0,  0},  //  25   W
{ 106,  0,  0,  0,  0},  //  26   E
{ 107,  0,  0,  0,  0},  //  27   R
{ 108,  0,  0,  0,  0},  //  28   T
{ 109,  0,  0,  0,  0},  //  29   Y
{ 110,  0,  0,  0,  0},  //  30   U
{ 111,  0,  0,  0,  0},  //  31   I
{ 100,  0,  0,  0,  0},  //  32   O
{ 101,  0,  0,  0,  0},  //  33   P
{  92,  0,  0,  0,  0},  //  34   [ {
{  93,  0,  0,  0,  0},  //  35   ] }				
{  57,  0,  0, 59,  0},  //  36   RETURN	ENTER		EXECUTE
{   0,  0,  0,  0,  0},  //  37  nil
{ 112,  0,  0,  0,  0},  //  38   A
{ 113,  0,  0,  0,  0},  //  39   S
{ 114,  0,  0,  0,  0},  //  40   D
{ 115,  0,  0,  0,  0},  //  41   F
{ 116,  0,  0,  0,  0},  //  42   G
{ 117,  0,  0,  0,  0},  //  43   H
{ 118,  0,  0,  0,  0},  //  44   J
{ 102,  0,  0,  0,  0},  //  45   K
{ 103,  0,  0,  0,  0},  //  46   L
{  94,  0,  0,  0,  0},  //  47   ; :
{  95,  0,  0,  0,  0},  //  48   ' "	
{   1,  0,  0,  51, 0},  //  49   ` ~	STEP
{   0,  0,  0,  0,  0},  //  50  nil
{  59,  0,  0,  0,  0},  //  51   \ |		execute
{ 120,  0,  0,  0,  0},  //  52   Z
{ 121,  0,  0,  0,  0},  //  53   X
{ 122,  0,  0,  0,  0},  //  54   C
{ 123,  0,  0,  0,  0},  //  55   V
{ 124,  0,  0,  0,  0},  //  56   B
{ 125,  0,  0,  0,  0},  //  57   N
{ 119,  0,  0,  0,  0},  //  58   M
{  96,  0,  0,  0,  0},  //  59   , <					
{  97,  0,  0,  0,  0},  //  60   . >
{  98,  0,  0,  0,  0},  //  61   / ?
{   0,  0,  0,  0,  0},  //  62  nil
{  71,  0,  0,  0,  0},  //  63   MULTIPLY
{   0,	0,  0,	0,  0},	 //  64	 nil
{  99,	0,  0,	0,  0},	 //  65	  SPACE
{  24,	0,  0,	0,  0},	 //  66	  CAPITAL
{  26,	0,  0,	0,  0},	 //  67	  F1			
{  27,	0,  0,	0,  0},	 //  68	  F2			
{  28,	0,  0,	0,  0},	 //  69	  F3			
{  32,	0,  0,	0,  0},	 //  70	  F4			
{  33,	0,  0,	0,  0},	 //  71	  F5			
{  29,	0,  0,	0,  0},	 //  72	  F6			
{  30,	0,  0,	0,  0},	 //  73	  F7			
{  31,	0,  0,	0,  0},	 //  74	  F8
{  36,	0,  0,	0,  0},	 //  75	  F9			
{  37,	0,  0,	0,  0},	 //  76	  F10					
{   0,	0,  0,	0,  0},	 //  77	 nil
{   0,	0,  0,	0,  0},	 //  78	 nil
{  72,	0,  0,	0,  0},	 //  79	  NUMPAD7
{  73,	0,  0,	0,  0},	 //  80	  NUMPAD8
{  74,	0,  0,	0,  0},	 //  81	  NUMPAD9
{  67,	0,  0,	0,  0},	 //  82	  SUBTRACT
{  68,	0,  0,	0,  0},	 //  83	  NUMPAD4
{  69,	0,  0,	0,  0},	 //  84	  NUMPAD5
{  70,	0,  0,	0,  0},	 //  85	  NUMPAD6
{  63,	0,  0,	0,  0},	 //  86	  ADD
{  64,	0,  0,	0,  0},	 //  87	  NUMPAD1
{  65,	0,  0,	0,  0},	 //  88	  NUMPAD2
{  66,	0,  0,	0,  0},	 //  89	  NUMPAD3
{  60,	0,  0,	0,  0},	 //  90	  NUMPAD0
{  61,	0,  0,	0,  0},	 //  91	  DECIMAL
{   0,	0,  0,	0,  0},	 //  92	 nil
{   0,	0,  0,	0,  0},	 //  93	 nil
{   0,	0,  0,	0,  0},	 //  94	 nil
{  52,	0,  0, 48,  0},	 //  95	 nil
{  53,	0,  0, 49,  0},	 //  96	 nil
{   0,	0,  0,	0,  0},	 //  97	 nil
{   0,	0,  0,	0,  0},	 //  98	 nil
{   0,	0,  0,	0,  0},	 //  99	 nil
{   0,	0,  0,	0,  0},	 // 100	 nil
{   0,	0,  0,	0,  0},	 // 101	 nil
{   0,	0,  0,	0,  0},	 // 102	 nil
{   0,	0,  0,	0,  0},	 // 103	 nil
{   0,	0,  0,	0,  0},	 // 104	 nil
{   0,	0,  0,	0,  0},	 // 105	 nil
{  75,	0,  0,	0,  0},	 // 106	  DIVIDE
{   0,	0,  0,	0,  0},	 // 107	 nil
{   0,	0,  0,	0,  0},	 // 108	 nil
{   0,	0,  0,	0,  0},	 // 109	 nil
{  45,	0,  0, 42,  0},	 // 110	  HOME		CLEAR->END	RECALL	
{  35,	0,  0, 49,  0},	 // 111	  UP		UP		ALPHA
{ 162,	0,  0, 53,  0},	 // 112	  PRIOR		shift up	RESULT
{  38,	0,  0, 56,  0},	 // 113	  LEFT		LEFT		PAUSE			
{  39,	0,  0, 58,  0},	 // 114	  RIGHT		RIGHT		CONTINUE
{  47,	0,  0, 52,  0},	 // 115	  END		RUN		CLR LN
{  34,	0,  0, 50,  0},	 // 116	  DOWN		DOWN		GRAPHICS 
{ 163,	0,  0, 48,  0},	 // 117	  NEXT		shift down	EDIT 
{  43,	0,  0, 40,  0},	 // 118	  INSERT	INS CHR		INS LN
{  44,	0,  0, 41,  0},	 // 119	  DELETE	DEL CHR		DEL LN
{   0,	0,  0,	0,  0},	 // 120	 nil
{   0,	0,  0,	0,  0},	 // 121	 nil
{   0,	0,  0,	0,  0},	 // 122	 nil
{   0,	0,  0,	0,  0},	 // 123	 nil
{   0,	0,  0,	0,  0},	 // 124	 nil
{   0,	0,  0,	0,  0},	 // 125	 nil
{   0,	0,  0,	0,  0},	 // 126	 nil
{   0,	0,  0,	0,  0},	 // 127	 nil
{   0,	0,  0,	0,  0},	 // 128	 nil

};

//
// current used keymap
//
static BYTE keycode[128][5];

VOID KnobRotate(SWORD knob) {
  if ((CK.ram[0x1C] != 0x7F) && (knob < 0)) CK.ram[0x1C] += knob;
  if ((CK.ram[0x1C] != 0x80) && (knob > 0)) CK.ram[0x1C] += knob;
#if defined DEBUG_KEYBOARDH
  k = sprintf(buffer,_T("%06X: KEYBOARD : knob (%04X) = %04X\n"), Chipset.Cpu.PC, knob, CK.ram[0x1C]);
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif
}

//
// Key released
//
VOID KeyboardEventUp(BYTE nId) {
  WORD key;

#if defined DEBUG_KEYBOARDH
  k = sprintf(buffer,_T("%06X: KEYBOARD : up key %04X shift %X\n"), Chipset.Cpu.PC, nId, CK.shift);
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif

  if (nState != SM_RUN)	return;		// not in running state, ignore key


  if (CK.altgr) {			// altgr
    key = keycode[nId][2];
  } else {
    key = keycode[nId][CK.shift + (CK.alt ? 3:0)];
    if (key == 0)
      key = keycode[nId][0 + (CK.alt ? 3:0)];
  }
  if (key != 0x00) {
    CK.ram[0x07] = 0x00;		// no more key pressed
  } 
}

//
// Key pressed
//
VOID KeyboardEventDown(BYTE nId) {
  BYTE key;

#if defined DEBUG_KEYBOARDH
  k = sprintf(buffer,_T("%06X: KEYBOARD : down key %04X shift %X\n"), Chipset.Cpu.PC, nId, CK.shift);
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif

  if (nState != SM_RUN)			// not in running state ?
    return;				// ignore key

  if (CK.altgr) {			// altgr
    key = keycode[nId][2];
  } else {
    key = keycode[nId][CK.shift + (CK.alt ? 3:0)];
    if (key == 0)
      key = keycode[nId][0 + (CK.alt ? 3:0)];
  }
  if (key != 0x00) {
    CK.kc_buffer[CK.hi_b++] = key;
  }  
}

//
// for pasting key to keyboard (to be done later)
// 
BOOL WriteToKeyboard(LPBYTE lpData, DWORD dwSize) {

  /* LPBYTE lpAd = lpData;
     DWORD  dwLen = dwSize;

     for ( ; dwLen > 0; dwLen--)
     {
     if (WaitForIdleState())
     {
     InfoMessage(_T("The emulator is busy."));
     return FALSE;
     }
     AsciiEventDown(*lpAd);
     bKeySlow = FALSE;
     Sleep(0);
     KeyboardEventUp(0x00);
     if (*lpAd == 0x0D)
     {
     if (lpAd[1] == 0x0A)
     lpAd++;
     }
     lpAd++;
     } */
  return TRUE;
}

//
// write to I/O space
//
BYTE Write_Keyboard(BYTE *a, WORD d, BYTE s) {
   
  if (s != 1) {
    fprintf(stderr,"Keyboard write acces not in byte !!");
    return BUS_ERROR;
  }
  switch (d) {
  case 0x8003:				// command
    if ((CK.status & 0x02) & (CK.stKeyb != 0)) {
      fprintf(stderr,"Command to Keyboard while busy !!");
    } else {
      CK.stKeyb = 10;			// do a command;
      CK.command = *(--a);
#if defined DEBUG_KEYBOARDH
      k = sprintf(buffer,_T("%06X: KEYBOARD : write command = %02X\n"), Chipset.Cpu.PC, CK.command);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    }
    return BUS_OK;
    break;
  case 0x8001:				// data from 68000 to 8041
    if (CK.status & 0x02) 
      fprintf(stderr,"Data to Keyboard while busy !!");
    CK.datain = *(--a);
    CK.stdatain = TRUE;			// strobe :)
    CK.status |= 0x02;			// busy till take it
    CK.stKeyb = 1;			// go take it
#if defined DEBUG_KEYBOARDH
    k = sprintf(buffer,_T("%06X: KEYBOARD : write data = %02X\n"), Chipset.Cpu.PC, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    return BUS_OK;
    break;
  default: 
    return BUS_ERROR;
    break;
  }
  return BUS_OK;
}

//
// read in I/O space
//
BYTE Read_Keyboard(BYTE *a, WORD d, BYTE s) {
  if (s != 1) {
    fprintf(stderr,"Keyboard read acces not in byte !!");
    return BUS_ERROR;
  }
  switch (d) {
  case 0x8001:				// data from 8041 to 68000
    *(--a) = CK.dataout;									
#if defined DEBUG_KEYBOARDH
    k = sprintf(buffer,_T("%06X: KEYBOARD : read data = %02X (%03d) \n"), Chipset.Cpu.PC, CK.dataout, CK.dataout);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    CK.int68000 = 0;			// clear interrupt
    CK.status &= 0xFE;			// clear interrupt
    return BUS_OK;
    break;
  case 0x8003:				// status
    *(--a) = CK.status;
    CK.status_cycles = 0; // read the status, keybord wait at least 1ms before doing something
#if defined DEBUG_KEYBOARDH
    k = sprintf(buffer,_T("%06X: KEYBOARD : read status = %02X\n"), Chipset.Cpu.PC, CK.status);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    return BUS_OK;
    break;
  default: 
    return BUS_ERROR;
    break;
  }
  return BUS_OK;
}

//
// init keyboard (copy the right keymap), set time if needed
//
VOID Init_Keyboard(VOID) { 
  WORD i,j;

  for (i = 0; i < 128; i++) {
    for (j = 0; j < 5; j++) {
	keycode[i][j] = uskeycode[i][j];
    }
  }
  if (Chipset.keeptime) SetHPTime();
#if defined DEBUG_KEYBOARDH
  k = sprintf(buffer,_T("	 : KEYBOARD : Init\n"));
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif
}

//
// reset keyboard
//
VOID Reset_Keyboard(VOID) {
  BYTE map = CK.keymap;

  bzero((BYTE *) &CK, sizeof(KEYBOARD));
  CK.keymap = map;			// keep keymap

  Init_Keyboard();
  CK.stKeyb = 90;			// jump to reset state
  CK.hi_b = 0x00;			// empty buffer
  CK.lo_b = 0x00;
  // doMouse = 0;
#if defined DEBUG_KEYBOARDH
  k = sprintf(buffer,_T("	 : KEYBOARD : Reset\n"));
  OutputDebugString(buffer); buffer[0] = 0x00;
#endif
}

//
// keyboard timers in 2.457 MHz cycles
//
VOID Do_Keyboard_Timers(DWORD cycles) {	// in 2.457MHz base clock

  CK.cycles += cycles;
  CK.status_cycles += cycles;
  if (CK.cycles >= 24570) {		// one centisecond elapsed
    CK.cycles -= 24570;

    if (CK.ram[0x07] != 0x00) {		// there is a key down ?
      if (CK.ram[0x22] != 0x00) {	// auto-repeat enabled ...
	CK.ram[0x21]++;			// auto-repeat timer
	if (CK.ram[0x21] == 0x00) {	// time to repeat
	  CK.ram[0x21] = CK.ram[0x22];	// reload
	  CK.ram[0x04] |= 0x20;			
	}
      }
    }

    CK.ram[0x25]--;			// knob timer
    if (CK.ram[0x25] == 0x00) {
      if (CK.ram[0x1C] != 0x00)		// knob moved
	CK.ram[0x05] |= 0x10;		// time to knob
      CK.ram[0x25] = CK.ram[0x26];	// reload
    }

    CK.ram[0x05] |= 0x04;		// time to do a PSI at 10 msec

    CK.ram[0x2D]++;			// adjust clock
    if (CK.ram[0x2D] == 0) {
      if ((Chipset.keeptime) && (CK.ram[0x30] == 0) && (CK.ram[0x31] == 0))
	SetHPTime();			// set time after bootrom erased it at boot (256 centisecond elapsed)
      CK.ram[0x2E]++;
      if ((CK.ram[0x2F] >= 0x83) && (CK.ram[0x2E] >= 0xD6)) {	// wrap on day
	CK.ram[0x2F] = 0x00;
	CK.ram[0x2E] = 0x00;
	CK.ram[0x30]++;		// one day more
	if (CK.ram[0x30] == 0)	// carry on
	  CK.ram[0x31]++;
      } else {
	if (CK.ram[0x2E] == 0) {
	  CK.ram[0x2F]++;
	}
      }
    }

    if (CK.ram[0x02] & 0x08) {		// non-maskable timer in use
      CK.ram[0x32]++;
      if (CK.ram[0x32] == 0x00) {
	CK.ram[0x33]++;
	if (CK.ram[0x33] == 0x00) {	// delay elapsed
	  CK.ram[0x05] |= 0x80;		// time to do it
	}
      }
    }
	
    if (CK.ram[0x02] & 0x10) {		// cyclic timer in use
      CK.ram[0x3A]++;
      if (CK.ram[0x3A] == 0x00) {
	CK.ram[0x3B]++;
	if (CK.ram[0x3B] == 0x00) {
	  CK.ram[0x3C]++;
	  if (CK.ram[0x3C] == 0x00) {			// delay elapsed
	    CK.ram[0x05] |= 0x08;			// time to do it
	    if ((CK.ram[0x1B] & 0x1F) != 0x1F) {	// not saturated, inc
	      BYTE data = (CK.ram[0x1B] & 0x1F) + 1;

	      CK.ram[0x1B] &= 0xE0;
	      CK.ram[0x1B] |= data;
	    }
	    CK.ram[0x1B] |= 0x20;
	    // reload
	    CK.ram[0x3A] = CK.ram[0x3D];
	    CK.ram[0x3B] = CK.ram[0x3E];
	    CK.ram[0x3C] = CK.ram[0x3F];
	  }
	}
      }
    }

    if (CK.ram[0x02] & 0x20) {			// delay timer in use
      CK.ram[0x37]++;
      if (CK.ram[0x37] == 0x00) {
	CK.ram[0x38]++;
	if (CK.ram[0x38] == 0x00) {
	  CK.ram[0x39]++;
	  if (CK.ram[0x39] == 0x00) {		// delay elapsed
	    CK.ram[0x05] |= 0x08;		// time to do it
	    CK.ram[0x1B] |= 0x40;			
	  }
	}
      }
    }

    if (CK.ram[0x02] & 0x40) {			// match timer in use
      if (CK.ram[0x36] == CK.ram[0x2F]) {
	if (CK.ram[0x35] == CK.ram[0x2E]) {
	  if (CK.ram[0x34] == CK.ram[0x2D]) {	// matched
	    CK.ram[0x05] |= 0x08;		// time to do it
	    CK.ram[0x1B] |= 0x80;						
	  }
	}
      }
    }

    if (CK.ram[0x02] & 0x80) {		// beeper in use
    }
  }
}

//
// state machine for 8041 keyboard microcontroller
//
VOID Do_Keyboard(VOID) {
  if (CK.shift)
    CK.ram[0x05] &= 0xFE;
  else 
    CK.ram[0x05] |= 0x01;
  if (CK.ctrl)
    CK.ram[0x05] &= 0xFD;
  else 
    CK.ram[0x05] |= 0x02;

  if (CK.ram[0x07] == 0x00)		// no key down
    CK.ram[0x21] = CK.ram[0x20];	// load key timer with auto-repeat delay
  if (CK.hi_b != CK.lo_b) {		// some key down ?
    CK.forceshift = CK.kc_buffer[CK.lo_b] >> 7;
    CK.ram[0x07] = CK.kc_buffer[CK.lo_b++] & 0x7F;
    CK.ram[0x04] |= 0x20;
  }

  switch (CK.stKeyb) {
  case 0:				// idle, wait for something
    CK.status &= 0xFD;			// clear busy bit
    if (CK.status_cycles > 2457) {	// one ms elapsed after status polled ?
      if (CK.int68000 == 0)		// no interrupt already pending
	CK.stKeyb = 1;			// yes, can now do something
    }
    break;
  case 1:
    if (CK.stdatain) {			// datain strobe ?
      CK.stdatain = FALSE;		// clear strobe
      if (CK.stKeybrtn != 0) {
	CK.stKeyb = CK.stKeybrtn;	// jump back to waiting command
	CK.stKeybrtn = 0;		// forget return
      } else {
	CK.status &= 0xFD;		// clear busy in case
      }
    } else if ((CK.ram[0x04] & 0x20) &&				// some key to send
	       (!(CK.ram[0x04] & 0x01))) {			// not masked
      CK.stKeyb = 30;		
    } else if ((CK.ram[0x05] & 0x04) &&				// PSI to send
	       (!(CK.ram[0x04] & 0x08))) {			// not masked
      CK.stKeyb = 50;
    } else if ((CK.ram[0x05] & 0x08) &&				// some special timer interrupts to do
	       (!(CK.ram[0x04] & 0x04))) {			// not masked
      CK.stKeyb = 50;
    } else if ((CK.ram[0x05] & 0x10) &&				// knob data to send 
	       (!(CK.ram[0x04] & 0x01))) {			// not masked
      CK.stKeyb = 40;						// send it
    } else if ((CK.ram[0x05] & 0x80) &&				// non-maskable interrup to do 
	       (!(CK.ram[0x04] & 0x10))) {			// not masked
      CK.stKeyb = 60;
    }
    break;
	
  case 10:						// got a command
    CK.status |= 0x02;					// set busy bit
    switch (CK.command) {
    case 0xAD:						// set time of day and date want 3 to 5 bytes
      CK.stKeyb = 0xAD0;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set time of day and date (3 or 5 bytes) \n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xAF:						// set date want 2 bytes
      CK.stKeyb = 0xAF0;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set date (2 bytes)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xB4:				// set real-time match want 3 bytes, cancel it if 0
      CK.stKeyb = 0xB40;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set real time match (3 bytes)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xB7:				// set delayed interrupt want 3 bytes, cancel it if 0
      CK.stKeyb = 0xB70;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set delayed interrupt (3 bytes)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xBA:				// set cyclic interrupt want 3 bytes, cancel if 0
      CK.stKeyb = 0xBA0;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set cyclic interrupt (3 bytes)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xB2:				// set up non-maskable timeout want 2 bytes, cancel if 0
      CK.stKeyb = 0xB20;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set non maskable timeout (2 bytes)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xA3:				// beep want 2 bytes
      CK.stKeyb = 0xA30;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Beep (2 bytes)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xA2:				// set auto-repeat rate want 1 byte
      CK.stKeyb = 0xA20;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set auto-repeat rate (1 byte)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xA0:				// set auto-repeat delay want 1 byte
      CK.stKeyb = 0xA00;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set auto-repeat delay (1 byte)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;
    case 0xA6:				// set knob pulse accumulating period want 1 byte
      CK.stKeyb = 0xA60;
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : %03X Set knob pulse accumulating period (1 byte)\n"), CK.stKeyb);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      break;

    default:
      if ((CK.command & 0xE0) == 0x40) {// mask interrupts
	CK.intmask = CK.command & 0x01F;
	CK.ram[0x04] &= 0xE0;
	CK.ram[0x04] |= CK.intmask;
	CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
	k = sprintf(buffer,_T("	       : KEYBOARD : %03X Mask interrupts : %02X\n"), CK.stKeyb, CK.intmask);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      } 
      else if ((CK.command & 0xE0) == 0x00) {	// read data and send interrupt level 1 to 68000
	CK.dataout = CK.ram[CK.command & 0x01F];
	CK.stKeyb = 20;
#if defined DEBUG_KEYBOARD
	k = sprintf(buffer,_T("	       : KEYBOARD : %03X read data (R%02X=%02X) and interrupt\n"), CK.stKeyb, CK.command & 0x01F, CK.dataout);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      }
      else if ((CK.command & 0xE0) == 0x20) {	// copy data to 0x13 to 0x17
	CK.ram[0x17] = CK.ram[CK.command & 0x03F];
	CK.ram[0x16] = CK.ram[(CK.command & 0x03F)-1];
	CK.ram[0x15] = CK.ram[(CK.command & 0x03F)-2];
	CK.ram[0x14] = CK.ram[(CK.command & 0x03F)-3];
	CK.ram[0x13] = CK.ram[(CK.command & 0x03F)-4];
	CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
	k = sprintf(buffer,_T("	       : KEYBOARD : %03X copy data from %02X-%02X to 0x13-0x17\n"), CK.stKeyb, (CK.command & 0x03F)-4, (CK.command & 0x03F));
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      }
      else {
#if defined DEBUG_KEYBOARD
	k = sprintf(buffer,_T("	       : KEYBOARD : %03X unknown\n"), CK.stKeyb);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	CK.stKeyb = 0;			 // unknown command
      }
      break;
    }
    break;
  case 20:				 // send a level one interrupt to 68000
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("	   : KEYBOARD : Send interrupt for data\n"));
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    CK.status &= 0x0E;
    CK.status |= 0x41;			// byte requested by 68000 with level one interrupt
    CK.int68000 = 1;
    CK.send_wait = 1;			// wait for read dataout before next action
    CK.stKeyb = 0;
    break;

  case 30:				// send a key
    if (CK.ram[0x07] != 0x00) {
      CK.status &= 0x0E;
      CK.status |= 0x81;		// some keycode with level one interrupt
      CK.status |= (CK.ctrl) ? 0x00 : 0x20;
      if (CK.forceshift) {		// change shift state
	CK.status |= (!CK.shift) ? 0x00 : 0x10;
      } else {
	CK.status |= (CK.shift) ? 0x00 : 0x10;
      }
      CK.dataout = CK.ram[0x07];
      CK.int68000 = 1;
      CK.send_wait = 1;			// wait for read dataout before next action
#if defined DEBUG_KEYBOARDH
      k = sprintf(buffer,_T("	     : KEYBOARD : Send interrupt for key : %02X,%02X\n"), CK.status, CK.dataout);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    }
    CK.ram[0x04] &= 0xDF;		// clear auto-repeat interrupt to do
    CK.stKeyb = 0;
    break;

  case 40:				// send a knob
    CK.status &= 0x0E;
    CK.status |= 0xC1;			// some knob with level one interrupt
    CK.status |= (CK.ctrl) ? 0x00 : 0x20;
    CK.status |= (CK.shift) ? 0x00 : 0x10;
    CK.dataout = CK.ram[0x1C];
    CK.ram[0x1C] = 0x00;		// clear knob count
    CK.int68000 = 1;
    CK.ram[0x05] &= 0xEF;		// clear knob interrupt to do 
    CK.send_wait = 1;			// wait for read dataout before next action
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDH
    k = sprintf(buffer,_T("	   : KEYBOARD : Send interrupt for knob : %02X,%02X\n"), CK.status, CK.dataout);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 50:				// send PSI and/or special timer
    CK.status &= 0x0E;			// clear PSI and special
    if ((CK.ram[0x05] & 0x04) && (!(CK.ram[0x04] & 0x08))) {
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("	     : KEYBOARD : Send interrupt for PSI\n"));
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      CK.ram[0x05] &= 0xFB;		// clear it
      CK.status |= 0x11;		// PSI with level one interrupt
    }
    if ((CK.ram[0x05] & 0x08) && (!(CK.ram[0x04] & 0x04))) {
#if defined DEBUG_KEYBOARD
      k = sprintf(buffer,_T("        : KEYBOARD : Send interrupt for special timer\n"));
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      CK.ram[0x05] &= 0xF7;		// clear it
      CK.status |= 0x21;		// special timer with level one interrupt
      CK.dataout = CK.ram[0x1B];
      CK.ram[0x1B] = 0x00;
    }
    CK.int68000 = 1;
    CK.ram[0x05] &= 0xEF;		// clear knob interrupt to do
    CK.send_wait = 1;			// wait for read dataout before next action
    CK.stKeyb = 0;
    break;

  case 60:				// send nmi at level seven interrupt to 68000
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : Send interrupt for nmi\n"));
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    CK.status &= 0x00;
    CK.status |= 0x04;			// level seven interrupt, as timeout, not reset key
    CK.int68000 = 7;
    CK.ram[0x05] &= 0x7F;		// clear it
    CK.stKeyb = 0;
    break;

  case 90:				// reset keyboard
    CK.cycles = 0;
    CK.ram[0x02] = 0x00;		// nothing counting
    CK.ram[0x04] = 0x1F;		// all masked
    CK.ram[0x11] = 0;			// US keyboard
    CK.stKeyb = 91;
    break;
  case 91:
    CK.cycles += Chipset.dcycles;
    if (CK.cycles > 40) CK.stKeyb = 92;	// 5 µs elapsed
    break;
  case 92:				// power-up (after 20 µsec)
    CK.status = 0x71;			// power-up and self test ok
    CK.dataout = 0x8E;
    CK.int68000 = 1;
    CK.stKeyb = 93;
    break;
  case 93:
    CK.cycles += Chipset.dcycles;
    if (CK.cycles > 200) CK.stKeyb = 94;// 20 µs elapsed
    break;
  case 94:
    CK.int68000 = 0;
    CK.stKeyb = 0;
    break;

  case 0xA00:				// set auto-repeat delay want 1 byte
    CK.stKeybrtn = 0xA01;
    CK.stKeyb = 0;
    break;
  case 0xA01:				// got a byte
    CK.ram[0x20] = CK.datain;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xA20:				// set auto-repeat rate want 1 byte
    CK.stKeybrtn = 0xA21;
    CK.stKeyb = 0;
    break;
  case 0xA21:				// got a byte
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    CK.ram[0x22] = CK.datain;
    CK.stKeyb = 0;
    break;


  case 0xA30:				// beep want 2 bytes
    CK.stKeybrtn = 0xA31;
    CK.stKeyb = 0;
    break;
  case 0xA31:				// got a byte
    CK.stKeybrtn = 0xA32;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    CK.ram[0x23] = CK.datain;
    break;
  case 0xA32:				// got a byte
    CK.ram[0x24] = CK.datain;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xA60:				// set knob pulse accumulating period want 1 byte
    CK.stKeybrtn = 0xA61;
    CK.stKeyb = 0;
    break;
  case 0xA61:				// got a byte
    CK.ram[0x26] = CK.datain;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xBA0:				// set cyclic interrupt want 3 bytes, cancel if 0
    CK.stKeybrtn = 0xBA1;
    CK.ram[0x02] &= 0xEF;		// clear cyclic interrupt in use
    CK.stKeyb = 0;
    break;
  case 0xBA1:				// got a byte
    CK.ram[0x3A] = CK.datain;
    CK.ram[0x3D] = CK.datain;
    CK.stKeybrtn = 0xBA2;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xBA2:				// got a byte
    CK.ram[0x3B] = CK.datain;
    CK.ram[0x3E] = CK.datain;
    CK.stKeybrtn = 0xBA3;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xBA3:				// got a byte
    CK.ram[0x3C] = CK.datain;
    CK.ram[0x3F] = CK.datain;
    CK.ram[0x02] |= 0x10;		// set cyclic interrupt in use
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xB70:				// set delayed interrupt want 3 bytes, cancel it if 0
    CK.stKeybrtn = 0xB71;
    CK.ram[0x02] &= 0xDF;		// clear delay interrupt in use
    CK.stKeyb = 0;
    break;
  case 0xB71:				// got a byte
    CK.ram[0x37] = CK.datain;
    CK.stKeybrtn = 0xB72;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xB72:				// got a byte
    CK.ram[0x38] = CK.datain;
    CK.stKeybrtn = 0xB73;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xB73:				// got a byte
    CK.ram[0x39] = CK.datain;
    CK.ram[0x02] |= 0x20;		// set delay interrupt in use
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARD
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xB40:				// set real-time match want 3 bytes, cancel it if 0
    CK.stKeybrtn = 0xB41;
    CK.ram[0x02] &= 0xBF;		// clear match interrupt in use
    CK.stKeyb = 0;
    break;
  case 0xB41:				// got a byte
    CK.ram[0x34] = CK.datain;
    CK.stKeybrtn = 0xB42;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xB42:				// got a byte
    CK.ram[0x35] = CK.datain;
    CK.stKeybrtn = 0xB43;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xB43:				// got a byte
    CK.ram[0x36] = CK.datain;
    CK.ram[0x02] |= 0x40;		// set match interrupt in use
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xB20:				// set up non-maskable timeout want 2 bytes, cancel if 0
    CK.stKeybrtn = 0xB21;
    CK.ram[0x02] &= 0xF7;		// clear nmi interrupt in use
    CK.stKeyb = 0;
    break;
  case 0xB21:				// got a byte
    CK.ram[0x32] = CK.datain;
    CK.stKeybrtn = 0xB22;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xB22:				// got a byte
    CK.ram[0x33] = CK.datain;
    CK.ram[0x02] |= 0x08;		// set nmi interrupt in use
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xAF0:				// set date want 2 bytes
    CK.stKeybrtn = 0xAF1;
    CK.stKeyb = 0;
    break;
  case 0xAF1:				// got a byte
    CK.ram[0x30] = CK.datain;
    CK.stKeybrtn = 0xAF2;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xAF2:				// got a byte
    CK.ram[0x31] = CK.datain;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xAD0:				// set time of day and date want 3 to 5 bytes
    CK.stKeybrtn = 0xAD1;
    CK.stKeyb = 0;
    break;
  case 0xAD1:				// got a byte
    CK.ram[0x2D] = CK.datain;
    CK.stKeybrtn = 0xAD2;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xAD2:				// got a byte
    CK.ram[0x2E] = CK.datain;
    CK.stKeybrtn = 0xAD3;
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;
  case 0xAD3:				// got a byte
    CK.ram[0x2F] = CK.datain;
    CK.stKeybrtn = 0xAF1;		// loop to date
    CK.stKeyb = 0;
#if defined DEBUG_KEYBOARDC
    k = sprintf(buffer,_T("        : KEYBOARD : %03X Got a byte : %02X\n"), CK.stKeybrtn, CK.datain);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    break;

  case 0xBB0:				// reset state
    break;

  default:
    break;
  }
}
