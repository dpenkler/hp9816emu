/*
 *   98x6.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   taken long ago from Emu42
//
//	 CpuEmulator main emulation loop, timers, debugger stuff 
//
//   Peripherals are are synchronous with 68000 (after each 68000 opcode)
//	 
//	 Can be changed, but be careful for HPIB ones.
//	 Timings are very delicate with the use of 9914A and the code used.
//	 The code addressing the TI9914A is different from BOOTROM, BASIC and PASCAL.
//	 It was very delicate to have something working for all three.
//

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"
#include <time.h>
#include <sys/time.h>

static BOOL  bCpuSlow = TRUE;		// enable/disable real speed

static long dwOldCyc;			// cpu cycles at last event
static long dwSpeedRef;		        // timer value at last event
static long dwTickRef;			// 1ms sample timer ticks

static long dwHpCount500;		// counter value for 500Hz timer
static long dwHpCount1000;		// counter value for 1000Hz timer
static long dwHp500;			// for 500Hz timer
static long dwHp1000;			// for 1000Hz timer

BOOL    bInterrupt = FALSE;		// no demand for interrupt
UINT    nState     = SM_INVALID;	// actual system state
UINT    nNextState = SM_RUN;		// next initial system state
WORD    wRealSpeed = 1;			// real speed emulation x1
//BOOL    bSound = FALSE;		// sound emulation activated ?

SYSTEM Chipset;

long dwMaxTimers   = 0;		// Max number of call to DoTimers before 2 ms elapsed 
long dwCyclesTimer = 16000;	// Number of MC68000 cycles before calling DoTimers at 500Hz for 8 MHz at full speed (will change)
long dwBaseRef     = 16000;	// Number of MC68000 cycles before calling DoTimers at 500Hz for 8 MHz for real speed
WORD wDivRef       = 1;		// divisor to get equivalent 8MHz cycles
long dwVsyncRef    = 133333;	// Number of 8 MHz MC68000 cycles for vsync at 60Hz 

//################
//#
//#    Low level subroutines
//#
//################

static inline long getTime() { // Returns timestamp in usecs
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC,&t);
  return (t.tv_sec * 1000000 + t.tv_nsec/1000);
}

//
// Adjust all timers: with capricorn cycles for real speed
// with High Performance counter otherwise.
// dtime is the time elapsed in 8Mhz ticks
//

static long dwNbCalls = 0;

static BOOL __forceinline DoTimers(long dtime) {
  long mtime,ttime;
  if (!bCpuSlow) {	// compute elapsed time with High Performance counter 

    dwNbCalls++;
    mtime = getTime();
    ttime = mtime - dwHpCount500;
    if (ttime < dwHp500) {	                // two ms elapsed ?
      dtime = 0;				// no
      if (dwNbCalls > dwMaxTimers) dwMaxTimers = dwNbCalls; // something ticks
      if (dwNbCalls > 1) dwCyclesTimer++;	// too much, raise  
      else               dwCyclesTimer--;       // not enough, lower
      dwVsyncRef = (dwCyclesTimer * 25);
      dwVsyncRef /= 3;
    } else {					// yes
      dtime  = ttime*1000 ;            	        // convert to 2.457 MHz ticks for kbd
      dtime /= 407; 
      dwHpCount500 = mtime;		        // ref for next call
      dwNbCalls = 0;								
    }
  } else {					// correct from 2ms base freq
    dtime = 1000 * dtime;
    dtime /= wDivRef;
    dtime /= 3255;				// 2.4576 MHz base
  }

  if (dtime > 0) {	// in 2.4576 MHz cycles
    Do_Keyboard_Timers(dtime);
    return TRUE;
  }
  return FALSE;
}

//
// adjust emulation speed
//
static int NTR=0;
static __forceinline VOID AdjustSpeed(VOID) {
  long dwTicks;
  long mtime,ttime;
  // cycles elapsed for next check
  if ((Chipset.cycles - dwOldCyc) >=  dwBaseRef) {	// enough cycles ?
    mtime = getTime();	                                // update
    ttime = mtime - dwSpeedRef;
    if (ttime <= dwHp500) {
      usleep(dwHp500 - ttime); // sleep for the remaining time
    }
    dwTicks = getTime() - dwSpeedRef;         // usec ticks elapsed
    // if last command sequence took over 50ms -> synchronize
    if(dwTicks > 50 * dwTickRef) {	      // time for last commands > 50ms (223 at 4474 Hz)
      fprintf(stderr,"NT workaround %d\n",NTR++);
      dwOldCyc = Chipset.cycles;	      // new synchronizing
      dwSpeedRef = getTime();	              // save new reference time
    } else {
      dwOldCyc   += dwBaseRef;		      // adjust cycles reference
      dwSpeedRef += dwHp500;		      // adjust reference time
    }
  } 
}

//################
//#
//#    Public functions
//#
//################

//
// Set HP time and date
// try to find if basic or pascal ...
// 
VOID SetHPTime(VOID)  {
  WORD	   system = 0;
  WORD	   data;
  BYTE	   chr;
  DWORD	   sysname = 0xFFFDC2;
  unsigned long  centisec, dw;	
  struct   tm *ts;
  time_t   mTime;
  BYTE    *t;
  
  while (1) {
    data = GetWORD(sysname);
    chr = (BYTE)(data >> 8);
    if (chr == 'B') system = 1;
    if (chr == 'P') system = 2;
    if (chr == 0x00) break;
    sysname++;
    if (system != 0) break;
    if (sysname == 0xFFFDCC) break;
    chr = (BYTE)(data & 0xFF);
    if (chr == 'B') system = 1;
    if (chr == 'P') system = 2;
    if (chr == 0x00) break;
    sysname++;
    if (system != 0) break;
    if (sysname == 0xFFFDCC) break;
  }
  if (system != 0) {
    mTime = time(NULL);
    ts = localtime(&mTime);
    centisec = ts->tm_sec+ts->tm_min*60+ts->tm_hour*3600; // number of seconds today
    centisec *= 100; // convert to centisecs
Add files via upload
9 minutes ago
README.md
Update README.md
2 hours ago
cpu9816.c
Add files via upload
9 minutes ago
cpu9816.h
Add files via upload
9 minutes ago
display.c
Add files via upload
9 minutes ago
display16.c
Add files via upload
9 minutes ago
dmpas.hpi
Add files via upload
9 minutes ago
fetch.c
Add files via upload
9 minutes ago
files.c
Add files via upload
9 minutes ago
hp-2225.c
Add files via upload
9 minutes ago
hp-7908.c
Add files via upload
9 minutes ago
hp-9121.c
Add files via upload
9 minutes ago
hp-9122.c
Add files via upload
9 minutes ago
hp-98620.c
Add files via upload
9 minutes ago
hp-98626.c
Add files via upload
9 minutes ago
hp-98635.c
Add files via upload
9 minutes ago
hp-ib.c
Add files via upload
9 minutes ago
hp9816emu.h
Add files via upload
9 minutes ago
keyboard.c
Add files via upload
9 minutes ago
kml.c
Add files via upload
9 minutes ago
kml.h
Add files via upload
9 minutes ago
mops.c
Add files via upload
9 minutes ago
mops.h
Add files via upload
9 minutes ago
opcodes.c
Add files via upload
9 minutes ago
opcodes.h
Add files via upload
9 minutes ago
ops.h
    Chipset.Keyboard.ram[0x2d] = (BYTE) (centisec & 0xFF);
    Chipset.Keyboard.ram[0x2e] = (BYTE) ((centisec >> 8) & 0xFF);
    Chipset.Keyboard.ram[0x2f] = (BYTE) ((centisec >> 16) & 0xFF);
    t = &Chipset.Keyboard.ram[0x2d];
    //    fprintf(stderr,"SetHPTime: %02x:%02x:%02x\n",t[0],t[1],t[2]);
    
    ts->tm_year += 1900; // For NT compatibility
    
    // calculate days: today from 01.01.1970 for pascal 3 & basic 2
    // calculate days: today from 01.01.1900 for basic 5
    dw = (DWORD) ts->tm_mon+1;
    if (dw > 2) dw -= 3L;
    else {
      dw += 9L;
      --ts->tm_year;
    }
    dw = (DWORD) ts->tm_mday + (153L * dw + 2L) / 5L;
    dw += (146097L * ((ts->tm_year) / 100L)) / 4L;
    dw += (1461L * ((ts->tm_year) % 100L)) / 4L;

    //fprintf(stderr,"SetHPTime SYS%c %02d:%02d:%02d   daycs %ld 0x%04lX\n",chr,ts->tm_hour,ts->tm_min,ts->tm_sec, centisec,dw);

    if (system == 2)  dw -= 719469L;			// for pascal (from 01.01.1970)
    if (system == 1)  dw -= 693961L;			// for basic  (from 01.01.1900)


    Chipset.Keyboard.ram[0x30] = (BYTE) (dw & 0xFF);
    Chipset.Keyboard.ram[0x31] = (BYTE) ((dw >> 8) & 0xFF);
  }
}

//
// adjust delay variables from speed settings
//
VOID SetSpeed(WORD speed) {			// set emulation speed
  fprintf(stderr,"Setting speed to %d\n",speed);
  dwSpeedRef = getTime();		        // save reference time
  switch (speed) {
  case 0:					// max speed
    dwOldCyc = Chipset.cycles;
    break;
  default:
  case 1:					// 8MHz
    dwBaseRef = 16000;
    dwVsyncRef = 133333;
    wDivRef = 1;
    break;
  case 2:					// 16MHz
    dwBaseRef = 32000;
    dwVsyncRef = 266667;
    wDivRef = 2;
    break;
  case 3:					// 24MHz
    dwBaseRef = 48000;
    dwVsyncRef = 400000;
    wDivRef = 3;
    break;
  case 4:					// 32MHz
    dwBaseRef = 64000;
    dwVsyncRef = 533333;
    wDivRef = 4;
    break;
  case 5:					// 40MHz
    dwBaseRef = 80000;
    dwVsyncRef = 666667;
    wDivRef = 5;
    break;
  }
  bCpuSlow = (speed != 0);			// save emulation speed 
}

//
// change thread state
//
UINT SwitchToState(UINT nNewState) {
  UINT nOldState = nState;

  if (nState == nNewState) return nOldState;
  // fprintf(stderr,"SwitchToState: %d, actual state: %d\n",nNewState, nState);
  switch (nState) {
  case SM_RUN: // Run
    switch (nNewState) {
    case SM_INVALID: // -> Invalid
      nNextState = SM_INVALID;
      bInterrupt = TRUE;			// leave inner emulation loop
      while (nState!=nNextState) usleep(10000);
      UpdateWindowStatus();
      break;
    case SM_RETURN: // -> Return
      nNextState = SM_INVALID;
      bInterrupt = TRUE;			// leave inner emulation loop
      while (nState!=nNextState) usleep(10000);
      nNextState = SM_RETURN;
      UpdateWindowStatus();
      break;
    }
    break;
  case SM_INVALID: // Invalid
    switch (nNewState) {
    case SM_RUN: // -> Run
      nNextState = SM_RUN;
      // don't enter opcode loop on interrupt request
      bInterrupt = FALSE;
      while (nState!=nNextState) usleep(10000);
      UpdateWindowStatus();
      break;
    case SM_RETURN: // -> Return
      nNextState = SM_RETURN;
      break;
    }
    break;
  }
  Chipset.annun &= ~1;
  Chipset.annun |= (nState==SM_RUN);
  UpdateAnnunciators(FALSE);
  return nOldState;
}

void *CpuEmulator(void * targ) {
  BYTE newI210 = 0;				// new interrupt level
  long mtime;
  
  dwHp1000  = 1000;                             // 1ms = 1000us
  dwHp500   = 2000;                             // 2ms = 2000us
  dwTickRef = 1000;		  	        // sample timer ticks 1ms

  initOP();					// init mc68000 op table

   loop:
  while (nNextState == SM_INVALID) {		// go into invalid state
    nState = SM_INVALID;			// in invalid state
    usleep(10000);                              // no point in burning up cycles for nothing
  }

  if (nNextState == SM_RETURN) {		// go into return state
    nState = SM_RETURN;			        // in return state
    return 0;					// kill thread
  }
  
  while (nNextState == SM_RUN) {
    if (nState != SM_RUN) {
      nState = SM_RUN;
      UpdateAnnunciators(FALSE);

      if (Chipset.keeptime) SetHPTime();	// update HP time & date

      // init speed reference
      dwOldCyc = (DWORD) (Chipset.cycles & 0xFFFFFFFF);
      mtime = getTime();
      dwSpeedRef    = mtime;
      dwHpCount500  = mtime;
      dwHpCount1000 = mtime;
    }

    while (!bInterrupt) {
      Chipset.dcycles = 0;
      EvalOpcode(newI210);	// execute opcode with interrupt if needed
      Chipset.cycles += (DWORD)(Chipset.dcycles);
      Chipset.ccycles += (DWORD)(Chipset.dcycles);

      if (Chipset.Cpu.reset) {
	if (szRomFileName[0]) MapRom(szRomFileName); // reload rom in case it got hosed
	Chipset.Cpu.reset = 0;
	Reset_Keyboard();
      }

      // display
      do_display_timers16(); 

      // HPIB controller
      DoHPIB();
      // HPIB Peripherals
      if (Chipset.Hpib70x == 1) DoHp9121(&Chipset.Hp9121);
      if (Chipset.Hpib72x == 3) DoHp9122(&Chipset.Hp9122);
      if (Chipset.Hpib73x != 0) DoHp7908(&Chipset.Hp7908_0);
      if (Chipset.Hpib74x != 0) DoHp7908(&Chipset.Hp7908_1);
      if (Chipset.Hpib71x != 0) DoHp2225(&Chipset.Hp2225);
      // keyboard
      Do_Keyboard();

      newI210 = (Chipset.Keyboard.int68000 == 7) ? 7: 
	(Chipset.Hpib.h_int) ? 3 :
	(Chipset.Keyboard.int68000 == 1) ? 1 : 0;

      if (!bCpuSlow) {	// compute elapsed time with High Performance counter ?
	if (Chipset.ccycles > dwCyclesTimer) {		// enough MC68000 cycles for 2 ms ?
	  if (DoTimers(Chipset.ccycles))		// ado it !
	    Chipset.ccycles -= dwCyclesTimer;
Add files via upload
9 minutes ago
README.md
Update README.md
2 hours ago
cpu9816.c
Add files via upload
9 minutes ago
cpu9816.h
Add files via upload
9 minutes ago
display.c
Add files via upload
9 minutes ago
display16.c
Add files via upload
9 minutes ago
dmpas.hpi
Add files via upload
9 minutes ago
fetch.c
Add files via upload
9 minutes ago
files.c
Add files via upload
9 minutes ago
hp-2225.c
Add files via upload
9 minutes ago
hp-7908.c
Add files via upload
9 minutes ago
hp-9121.c
Add files via upload
9 minutes ago
hp-9122.c
Add files via upload
9 minutes ago
hp-98620.c
Add files via upload
9 minutes ago
hp-98626.c
Add files via upload
9 minutes ago
hp-98635.c
Add files via upload
9 minutes ago
hp-ib.c
Add files via upload
9 minutes ago
hp9816emu.h
Add files via upload
9 minutes ago
keyboard.c
Add files via upload
9 minutes ago
kml.c
Add files via upload
9 minutes ago
kml.h
Add files via upload
9 minutes ago
mops.c
Add files via upload
9 minutes ago
mops.h
Add files via upload
9 minutes ago
opcodes.c
Add files via upload
9 minutes ago
opcodes.h
Add files via upload
9 minutes ago
ops.h
	}
      } else						// slow mode at x MHz with cycles
	if (Chipset.ccycles > dwBaseRef) {		// at least x*2000 MC68000 cycles ?
	  DoTimers(Chipset.ccycles);
	  Chipset.ccycles -= dwBaseRef;
	}
      if (bCpuSlow)			// emulation slow down
	AdjustSpeed();			// adjust emulation speed 
    }
    bInterrupt = FALSE;			// be sure to reenter opcode loop
  }
  goto loop;
}
