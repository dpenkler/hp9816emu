/*
 *   Hp-9122.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   HPSS80 floppy disk as ADDRESS 0 simulation 
//	 710KB per unit normally
//   SS80 protocol
//	 1 device 2 unit
//	 describe value from HP9123 docs
//
//	 same code base as hp-7908.c
//
//   format option	bytes/sector	useable tracks	sectors/track	sides	capacity KB
//	0		256			77		16	2		616
//	1		256			77		16	2		616
//	2		512			77		 9	2		693
//	3	       1024			77		 5	2		770
//	4		256			33		16	2		264	normal old one

#include "common.h"
#include "hp9816emu.h"

#define HP9122_ID1 0x02
#define HP9122_ID2 0x22

//#define DEBUG_HP9122						// debug flag

#define DELAY_CMD 50						// delay fo command response

#if defined DEBUG_HP9122
	TCHAR buffer[256];
	int k;
#endif

//
// HPSS80 controller variables
// geometry format
//
static int disk_bytes[5] = {256 * 77 * 16 * 2,		// 0
			    256 * 77 * 16 * 2,		// 1
			    512 * 77 * 9 * 2,		// 2
			    1024 * 77 * 5 * 2,		// 3
			    256 * 33 * 16 * 2};		// 4
static int disk_sectors[5] = {77 * 16 * 2,		// 0
			      77 * 16 * 2,		// 1
			      77 * 9 * 2,		// 2
			      77 * 5 * 2,		// 3
			      33 * 16 * 2};		// 4

//################
//#
//#    Low level subroutines
//#
//################
#include <sys/time.h>
static long timeGetTime() {
  struct timeval tv;
  long res;
  gettimeofday(&tv,NULL);
  res = tv.tv_sec*1000+tv.tv_usec/1000;
  return res;
}

//
// get lifname from volume in unit
//
static VOID GetLifName(HPSS80 *ctrl, BYTE unit) {
  BYTE i = 0;

  if (ctrl->disk[unit] == NULL) {
    ctrl->lifname[unit][0] = 0x00;
    return;
  }
  while (i < 6) {
    if (ctrl->disk[unit][i+2] == 0x00) {
      ctrl->lifname[unit][i] = 0x00;
      i = 7;
    } else {
      ctrl->lifname[unit][i] = ctrl->disk[unit][i+2];
      i++;
    }
  }
  if (i == 6) ctrl->lifname[unit][i] = 0x00;
}

//
// adjust sector address of unit if overflow
//
static VOID raj_addr(HPSS80 *ctrl, BYTE u) {
  while (ctrl->sector[u] >= ctrl->nsectors[u])	    ctrl->sector[u]   -= ctrl->nsectors[u];
  while (ctrl->head[u] >= ctrl->nheads[u])	    ctrl->head[u]     -= ctrl->nheads[u];
  while (ctrl->cylinder[u] >= ctrl->ncylinders[u])  ctrl->cylinder[u] -= ctrl->ncylinders[u];
  ctrl->addr[u] = (ctrl->sector[u] + 
		   ctrl->head[u] * ctrl->nsectors[u] + 
		   ctrl->cylinder[u] * ctrl->nheads[u] * ctrl->nsectors[u]
		   ) * ctrl->nbsector[u];
}

//
// increment sector address of unit
//
static VOID inc_addr(HPSS80 *ctrl, BYTE u) {
  ctrl->sector[u]++;
  if (ctrl->sector[u] >= ctrl->nsectors[u]) {
    ctrl->sector[u] = 0;
    ctrl->head[u]++;
    if (ctrl->head[u] >= ctrl->nheads[u]) {
      ctrl->head[u] = 0;
      ctrl->cylinder[u]++;
      if (ctrl->cylinder[u] >= ctrl->ncylinders[u]) ctrl->cylinder[u] = 0;	// wrap
    }
  }
  raj_addr(ctrl, u);
}

//
// get a command byte from circular buffer with delay
//
static BOOL pop_c(HPSS80 *ctrl, BYTE *c) {
  if (ctrl->hc_hi == ctrl->hc_lo)  return FALSE;	// no command
  if (ctrl->hc_t[ctrl->hc_lo] != 0) {			// wait more
    ctrl->hc_t[ctrl->hc_lo]--;
    return FALSE;
  }
  *c = ctrl->hc[ctrl->hc_lo++];				// get a command byte
  ctrl->hc_lo &= 0x3FF;					// wrap circular buffer
  return TRUE;
}

//
// get a data byte from circular buffer with delay
//
static BOOL pop_d(HPSS80 *ctrl, BYTE *c, BYTE *eoi) {
  if (ctrl->hd_hi == ctrl->hd_lo)			// no data
    return FALSE;
  if (ctrl->hd_t[ctrl->hd_lo] != 0) {		// wait more
    ctrl->hd_t[ctrl->hd_lo]--;
    return FALSE;
  }
  *c = (BYTE) (ctrl->hd[ctrl->hd_lo] & 0x00FF);	// get a data byte
  *eoi = (BYTE) (ctrl->hd[ctrl->hd_lo++] >> 8);	// get eoi state
  ctrl->hd_lo &= 0x3FF;				// wrap circular buffer
  return TRUE;
}

//
// controller -> disk command byte
// use of delay to avoid race with 68000 code driving the ti9914A
//
BOOL hp9122_push_c(VOID *controler, BYTE c) {	// push on stack hc_9122
  HPSS80 *ctrl = (HPSS80 *) controler;

  c &= 0x7F;						// remove parity

  if (c == 0x20 + ctrl->hpibaddr) {			// my listen address
    ctrl->listen = TRUE;
    ctrl->untalk = FALSE;
  } else if (c == 0x40 + ctrl->hpibaddr) {		// my talk address
    ctrl->talk = TRUE;
    ctrl->untalk = FALSE;
  } else if (c == 0x3F) {				// unlisten
    ctrl->listen = FALSE;
    ctrl->untalk = FALSE;
  } else if (c == 0x5F) {				// untalk
    ctrl->talk = FALSE;
    ctrl->untalk = TRUE;
  } else if ((c & 0x60) == 0x20) {			// listen other, skip
    ctrl->untalk = FALSE;
  } else if ((c & 0x60) == 0x40) {			// talk other, skip
    ctrl->untalk = FALSE;
  } else {						// other
    if ((c == 0x60 + ctrl->hpibaddr) && ctrl->untalk) {	// my secondary address after untak -> identify
      ctrl->untalk = FALSE;
      ctrl->stss80 = 100;
    } else {
      ctrl->untalk = FALSE;
      if (ctrl->talk || ctrl->listen) {	// ok
#if defined DEBUG_HP9122
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9122:%d: got %02X command L:%d, T:%d\n"), Chipset.Cpu.PC, ctrl->hpibaddr, c, ctrl->listen, ctrl->talk);
	  OutputDebugString(buffer);
	}
#endif
	ctrl->hc_t[ctrl->hc_hi] = 10;	// 10 mc68000 op cycles of transmission delay
	ctrl->hc[ctrl->hc_hi++] = c;
	ctrl->hc_hi &= 0x3FF;		// wrap circular buffer
	_ASSERT(ctrl->hc_hi != ctrl->hc_lo);
      }
    }

  }
  return TRUE;
}

//
// controller -> disk data byte
// use of delay to avoid race with 68000 code driving the ti9914A
//
BOOL hp9122_push_d(VOID *controler, BYTE d, BYTE eoi) {		// push on stack hd_9122
  HPSS80 *ctrl = (HPSS80 *) controler;

  if (ctrl->listen || ctrl->talk) {				// only if listen or talk state
    if (((ctrl->hd_hi + 1) & 0x3FF) == ctrl->hd_lo)		// if full, not done
      return FALSE;
    else {
      ctrl->hd_t[ctrl->hd_hi] = 10;				// 10 mc68000 op cycles of transmission delay
      ctrl->hd[ctrl->hd_hi++] = (WORD) (d | (eoi << 8));
      ctrl->hd_hi &= 0x3FF;
      _ASSERT(ctrl->hd_hi != ctrl->hd_lo);
      return TRUE;
    }
  } else 
    return TRUE;
}

//################
//#
//#    Public functions
//#
//################

//
//   FSA to simulate the disc controller (SS80 protocol)
//

// for led display
static BYTE leds[] = {0, 0, 5, 0, 6};

VOID DoHp9122(HPSS80 *ctrl) {
  DWORD dw;
  BYTE eoi;

  if (ctrl->stss80 > 1) {										
    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr + 0 * 2]));		// unit 0 led off
    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr + 1 * 2]));		// unit 1 led off
    if (ctrl->unit != 15) {
      Chipset.annun |= (1 << (leds[ctrl->hpibaddr + ctrl->unit * 2]));	// unit active, light led
    }
  }
  switch (ctrl->stss80) {
  case 0:								// IDLE STATE
    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr + 0 * 2]));		// unit 0 led off
    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr + 1 * 2]));		// unit 1 led off
    ctrl->stss80++;
    break;
  case 1:
    if (pop_c(ctrl, &ctrl->c)) {
      if (ctrl->hd_hi != ctrl->hd_lo)
	fprintf(stderr,"9122 command with data pending ... !!");
      ctrl->stss80 = 2;				// if there is a command ...
    }
    break;
  case 2:
    if (ctrl->talk) {
      switch(ctrl->c) {
      case 0x6E:				// secondary 0x0E	: Normal execution, send data
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 2000;			// even with qstat != 0
	break;
      case 0x70:				// secondary 0x10   : Reporting phase, stand alone QSTAT = QSTAT
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MTA : SEC10 : Report QSTAT %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->qstat[ctrl->unit]);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 2100;
	break;
      case 0x72:				// secondary 0x12	: Transparent execution
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 2200;
	break;
      default:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MTA : Unknown secondary %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	//						_ASSERT(0);
	ctrl->stss80 = 0;				// restart
	break;
      }
    } else if (ctrl->listen) {
      switch(ctrl->c) {
      case 0x04:					// SDC selected device clear
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 1202;
	break;
      case 0x65:					// secondary 0x05	: Main group of command
#if defined DEBUG_HP9122H
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA : SEC5 : Main command\n"), Chipset.Cpu.PC, ctrl->hpibaddr, c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 1000;
	break;
      case 0x6E:					// secondary 0x0E	: Execution phase
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 1100;
	break;
      case 0x70:					// secondary 0x10   : Amigo clear
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA : SEC10 : Amigo clear\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 1200;
	break;
      case 0x72:					// secondary 0x12   : Transparent command
#if defined DEBUG_HP9122H
	k = wsprintf(buffer,_T("%06X: HP9122:%d : MLA : SEC12 : Transparent command\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->ppol_e = FALSE;
	ctrl->stss80 = 1300;
	break;
      default:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA : Unknown secondary %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	//						_ASSERT(0);
	ctrl->stss80 = 0;				// restart
	break;
      }
    }
    break;
    // receive my secondary address
  case 100:
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: UNT MSA Identify\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    ctrl->count = DELAY_CMD * 4;			// wait a bit
    ctrl->stss80++;
    break;
  case 101:
    if (ctrl->count == 1)
      ctrl->stss80++;
    ctrl->count--;
    break;
  case 102:
    if (h_push((BYTE) (HP9122_ID1), 0))		// even if talk not enable .... (in fact talk enable on 31 (secondary shadow address of all amigo device)
      ctrl->stss80++;	
    break;
  case 103:
    if (h_push((BYTE) (HP9122_ID2), 1))		// with EOI
      ctrl->stss80 = 0;
    break;

    // MLA & secondary 05		// main group of command
  case 1000:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      switch (ctrl->c) {
      case 0x00:					// op 0x00 : Locate and read
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 00 Locate and read\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3000;
	break;
      case 0x02:					// op 0x02 : Locate and write
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 02 Locate and write\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3100;
	break;
      case 0x04:					// op 0x04 : Locate and verify, do nothing
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 04 Locate and verify\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3200;
	break;
      case 0x06:					// op 0x06 : Spare block
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 06 Spare block\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3300;
	break;
      case 0x0D:					// op 0x0D : Request status
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 0D Request status\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3400;
	break;
      case 0x0E:					// op 0x0E : Release (no op)
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 0E Release (no op)\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->ppol_e = TRUE;
	break;
      case 0x0F:					// op 0x0F : Release denied (no op)
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 0F Release denied (no op)\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->ppol_e = TRUE;
	break;
      case 0x10:					// op 0x10 : complementary : Set address
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 10 Set address\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->word = 0;			// 2 bytes
	ctrl->dword = 0;		// 4 bytes
	ctrl->count = 5;		// 6 bytes, keep 4 only
	ctrl->stss80 = 3500; 
	break;
      case 0x18:					// op 0x18 : complementary : Set Length
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 18 Set length\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->dword = 0;		// 4 bytes
	ctrl->count = 3;		// 4 bytes
	ctrl->stss80 = 3600;
	break;
      case 0x20:			// unit 0
      case 0x21:			// unit 1
      case 0x2F:			// unit 15 if needed (not for a real 9122)
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 2%X Set Unit %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x0F, ctrl->c & 0x0F);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->unit = ctrl->c & 0x0F;
	break;
      case 0x22:			// other unit -> error
      case 0x23:
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
      case 0x28:
      case 0x29:
      case 0x2A:
      case 0x2B:
      case 0x2C:
      case 0x2D:
      case 0x2E:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 2%X Set Unit %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x0F, ctrl->c & 0x0F);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (!ctrl->mask[ctrl->unit].module_addressing)
	  ctrl->err[ctrl->unit].module_addressing = 1;
	break;
      case 0x31:			// op 0x31 : Initiate Utility : validate, format, download
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31 ...\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3700;
	break;
      case 0x33:					// op 0x33 : Initiate diagnosis
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 33 Initiate diagnosis\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3800;
	break;
      case 0x34:			// op 0x34 : no op
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 34 no op\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	break;
      case 0x35:					// op 0x35 : complementary : Describe
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 35 Describe\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 3900;
	break;
      case 0x37:			// op 0x37 : Initialize media
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 37 Initialize media\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 4000;
	break;
      case 0x39:			// op 0x39 : complementary : Set rps (no op)					
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 39 Set rps (no op)\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->count = 2;		// skip 2 bytes
	ctrl->stss80 = 4300;
	break;
      case 0x3B:			// op 0x3B : complementary : Set release (no op)
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 3B Set release (no op)\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->count = 1;		// skip one byte
	ctrl->stss80 = 4300;
	break;
      case 0x3E:			// op 0x3E : complementary : Set status mask
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 3E Set status mask\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->count = 0;
	ctrl->stss80 = 4100;
	break;
      case 0x40:			// volume 0
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 4%X Set Volume %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x07, ctrl->c & 0x07);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->volume = 0;
	break;
      case 0x41:			// other volume -> error
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x45:
      case 0x46:
      case 0x47:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 4%X Set Volume %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x07, ctrl->c & 0x07);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (!ctrl->mask[ctrl->unit].module_addressing)
	  ctrl->err[ctrl->unit].module_addressing = 1;
	break;
      case 0x48:		       // op 0x48 : complementary : Set return addressing mode					
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: Set return addressing mode\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 4200;
	break;
      case 0x4C:			// op 0x4C : Door unlock here no door
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 4C Door unlock\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (ctrl->unit == 15) {
	  if (!ctrl->mask[ctrl->unit].illegal_opcode)
	    ctrl->err[ctrl->unit].illegal_opcode = 1;
	}
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 0;
      case 0x4D:			// op 0x4D : Door lock here no door
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 4D Door lock\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (ctrl->unit == 15) {
	  if (!ctrl->mask[ctrl->unit].illegal_opcode)
	    ctrl->err[ctrl->unit].illegal_opcode = 1;
	}
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 0;
      default:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: Unknown opcode %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	//				_ASSERT(0);
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 0;		// restart
	break;
      }
    } else if (!ctrl->listen) {		// got unlisten ... end of sequence
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;
    } else if (pop_c(ctrl, &ctrl->c)) {	// got a command
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 2;
    }
    break;
    // MLA & secondary 0E	// execution phase 
  case 1100:
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SECE: Exec phase(%d)\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->rwvd);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    switch (ctrl->rwvd) {
    case 2:				// write phase, wait for data
      ctrl->stss80 = 2400;
      break;
    case 3:				// verify phase, wait for data
      ctrl->stss80 = 2500;
      break;
    case 5:				// data phase for format & all
      ctrl->stss80 = 2550;
      break;
    default:
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SECE: unknown phase(%d)\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->rwvd);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      if (ctrl->qstat[ctrl->unit] == 0)
	fprintf(stderr,"9122 Listen exec phase 0 without errors ... !!");
      ctrl->stss80 = 0;		// loop to read all data
      break;
    }
    ctrl->rwvd = 0;	// exec done
    break;

    // MLA & secondary 10 : HP-300 - Amigo clear
  case 1200:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// get dummy byte
      ctrl->stss80++;
    }
    break;
  case 1201:
    if (pop_c(ctrl, &ctrl->c)) {	// get Selected Device Clear command
      ctrl->stss80++;
    }
    break;
  case 1202:
    ctrl->count = DELAY_CMD;
    ctrl->stss80++;
    break;
  case 1203:
    if (ctrl->count == 1)
      ctrl->stss80++;
    ctrl->count--;
    break;
  case 1204:
    ctrl->unit = 15;
    ctrl->stss80 = 5301;				// do a channel clear on unit 15
    break;
		
    // MLA & secondary 12	 // Transparents operations
  case 1300:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      switch (ctrl->c) {
      case 0x01:					// op 0x01 : HP-IB Parity checking
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 01 HPIB parity checking\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 5000;				
	break;
      case 0x02:					// op 0x02 : Read Loopback
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 02 Read loopback\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 5100;
	break;
      case 0x03:					// op 0x03 : Write Loopback
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 03 Write loopback\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 5200;
	break;
      case 0x08:					// op 0x08 : Channel independant clear
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 08 Channel independant clear\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 5300;
	break;
      case 0x09:					// op 0x09 : Cancel
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 09 Cancel\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 5400;
	break;
      case 0x20:					// complementary set unit 0
      case 0x21:					// unit 1
      case 0x2F:					// unit 15
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 2%X Set Unit %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x0F, ctrl->c & 0x0F);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->unit = ctrl->c & 0x0F;
	break;
      case 0x22:					// other unit -> error
      case 0x23:
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
      case 0x28:
      case 0x29:
      case 0x2A:
      case 0x2B:
      case 0x2C:
      case 0x2D:
      case 0x2E:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 2%X Set Unit %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x0F, ctrl->c & 0x0F);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (!ctrl->mask[ctrl->unit].module_addressing)
	  ctrl->err[ctrl->unit].module_addressing = 1;
	break;
      case 0x34:					// op 0x34 : no op
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 34 no op\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	break;
      case 0x40:					// complementary Set volume 0
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 4%X Set Volume %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x07, ctrl->c & 0x07);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->volume = 0;
	break;
      case 0x41:					// other volume -> error
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x45:
      case 0x46:
      case 0x47:
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: OP 4%X Set Volume %X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c & 0x07, ctrl->c & 0x07);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (!ctrl->mask[ctrl->unit].module_addressing);
	ctrl->err[ctrl->unit].module_addressing = 1;
	break;
      default:					// others ... loop 
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC12: unknown op %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	break;
      }
    } else if (!ctrl->listen) {			// got unlisten ... end of sequence
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;
    } else if (pop_c(ctrl, &ctrl->c)) {	// got a command
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 2;
    }
    break;


    // MTA & secondary 0E		// normal execution phase 
  case 2000:
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: MTA SECE: Exec phase(%d)\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->rwvd);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    switch (ctrl->rwvd) {
    case 1 :			// read phase, send disk data back
      ctrl->stss80 = 2300;
      break;
    case 4:				// data phase, send local buffer data
      ctrl->stss80 = 2600;
      break;
    default:			// send one byte tagged with EOI ... to finish an erronous exec
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MTA SECE: unknown phase(%d)\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->rwvd);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      if (ctrl->length[ctrl->unit] == 0) {
	if (!ctrl->mask[ctrl->unit].message_sequence)
	  ctrl->err[ctrl->unit].message_sequence = 1;
      }
      if (ctrl->qstat[ctrl->unit] == 0)
	fprintf(stderr,"9122 Talking exec phase 0 without errors ... !!");
      ctrl->stss80++;
      break;
    }
    ctrl->rwvd = 0;		// exec done
    break;
  case 2001:
    ctrl->count = DELAY_CMD * 4;
    ctrl->stss80++;
    break;
  case 2002:
    if (ctrl->count == 1)
      ctrl->stss80++;
    ctrl->count--;
    break;
  case 2003:
    if (ctrl->talk) {
      if (h_push(1, 0x80)) {				// send 1 tagged with EOI
	ctrl->count = DELAY_CMD;
	ctrl->stss80 = 2002;				// loop and wait a bit
      }
    } else {							
      ctrl->ppol_e = TRUE;				// ok not talker anymore
      ctrl->stss80 = 0;
    }
    break;

    // MTA & secondary 10		// reporting phase QSTAT
  case 2100:
    ctrl->count = DELAY_CMD * 4;
    ctrl->stss80++;
    break;
  case 2101:
    if (ctrl->count == 1)
      ctrl->stss80++;
    ctrl->count--;
    break;
  case 2102:
    if (ctrl->talk)
      if (h_push(ctrl->qstat[ctrl->unit], 0x80))		// send QSTAT = 0 and EOI
	ctrl->stss80 = 0;					// do not enable parallel poll back
    break;

    // MTA & secondary 12		// transparent execution
  case 2200:
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: MTA SEC12: transparent execution phase(%d)\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->rwvd);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    _ASSERT(0);
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 0;				// restart
    break;


  case 2300:				// exec phase, send data readed from disk
    ctrl->dcount = ctrl->length[ctrl->unit];
    ctrl->count = ctrl->nbsector[ctrl->unit];
    ctrl->count = DELAY_CMD * 4;
    ctrl->stss80++;
    break;
  case 2301:
    if (ctrl->count == 1)
      ctrl->stss80++;
    ctrl->count--;
    break;
  case 2302:
    if (ctrl->dcount == 1)	{	// last byte
      if (h_push(ctrl->disk[ctrl->unit][ctrl->addr[ctrl->unit]], 0x80)) {		// with EOI
	ctrl->dcount--;
	ctrl->count--;
	ctrl->addr[ctrl->unit]++;
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 0;
      }
    } else {
      if (h_push(ctrl->disk[ctrl->unit][ctrl->addr[ctrl->unit]], 0x0)) {		// without EOI
	ctrl->dcount--;
	ctrl->count--;
	ctrl->addr[ctrl->unit]++;
	ctrl->stss80++;
      }
    }
    break;
  case 2303:
    if (ctrl->count == 0) {						// last from sector sended
      ctrl->count = ctrl->nbsector[ctrl->unit];
      ctrl->address[ctrl->unit]++;
      if (ctrl->address[ctrl->unit] == (disk_sectors[ctrl->type[ctrl->unit]]))
	ctrl->address[ctrl->unit] = 0;		// wrap
    }
    ctrl->stss80 = 2302;
    break;

  case 2400:				// exec phase, get data for writing disk
    ctrl->dcount = ctrl->length[ctrl->unit];
    ctrl->count = ctrl->nbsector[ctrl->unit];
    ctrl->stss80++;
    break;
  case 2401:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      ctrl->disk[ctrl->unit][ctrl->addr[ctrl->unit]] = ctrl->c;
      ctrl->dcount--;
      ctrl->count--;
      ctrl->addr[ctrl->unit]++;
      if (eoi) ctrl->dcount = 0;
      if (ctrl->count == 0) {						// last from sector sended
	ctrl->count = ctrl->nbsector[ctrl->unit];
	if (ctrl->address[ctrl->unit] == 0x00000000) {
	  GetLifName(ctrl, ctrl->unit);
	  emuUpdateButton(ctrl->hpibaddr, ctrl->unit, ctrl->lifname[ctrl->unit]);
	}
	ctrl->address[ctrl->unit]++;
	if (ctrl->address[ctrl->unit] == (disk_sectors[ctrl->type[ctrl->unit]]))
	  ctrl->address[ctrl->unit] = 0;	// wrap
      }
      if (ctrl->dcount == 0) {
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 0;
      }
    }
    break;
  case 2500:				// exec phase, get data for verifying disk
    _ASSERT(0);
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 0;
    break;

  case 2550:				// exec phase, get data for format & all in data
    ctrl->count = 0;
    ctrl->stss80++;
    break;
  case 2551:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      ctrl->data[ctrl->count++] = ctrl->c;
      //if (eoi) {	// not seen ??
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 3711;		// go back
      //}
    }
    break;

  case 2600:				// exec phase, send data from buffer
    ctrl->dindex = 0;
    ctrl->count = DELAY_CMD * 4;
    ctrl->stss80++;
    break;
  case 2601:
    if (ctrl->count == 1)
      ctrl->stss80++;
    ctrl->count--;
    break;
  case 2602:
    if (ctrl->dindex < ctrl->dcount) {
      if (ctrl->dindex == (ctrl->dcount-1)) {
	if (h_push(ctrl->data[ctrl->dindex], 0x80))		// with EOI
	  ctrl->dindex++;
      } else {
	if (h_push(ctrl->data[ctrl->dindex], 0))
	  ctrl->dindex++;
      }
    } else {
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;
    }
    break;


    // MLA & secondary 05 , op 0x00 : Locate and read
  case 3000:
    if (ctrl->unit < 2)
      ctrl->stss80++;
    else {
      if (!ctrl->mask[ctrl->unit].illegal_opcode)
	ctrl->err[ctrl->unit].illegal_opcode = 1;
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;
    }
    break;
  case 3001:
    // check for new medium
    if (ctrl->new_medium[ctrl->unit]) {
      if (!ctrl->mask[ctrl->unit].power_fail)
	ctrl->err[ctrl->unit].power_fail = 1; 
      ctrl->new_medium[ctrl->unit] = 0;
    }
    // check for disk present
    if (ctrl->disk[ctrl->unit] == NULL) {
      if (!ctrl->mask[ctrl->unit].not_ready)
	ctrl->err[ctrl->unit].not_ready = 1;
    } else {
      ctrl->addr[ctrl->unit] = ctrl->address[ctrl->unit] * ctrl->nbsector[ctrl->unit];
      if (ctrl->length[ctrl->unit] != 0)
	ctrl->rwvd = 1;		// do a read on next execution
    }
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 0;
    break;

    // MLA & secondary 05 , op 0x02 : Locate and write
  case 3100:
    if (ctrl->unit < 2)
      ctrl->stss80++;
    else {
      if (!ctrl->mask[ctrl->unit].illegal_opcode)
	ctrl->err[ctrl->unit].illegal_opcode = 1;
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;
    }
    break;
  case 3101:
    // check for new medium
    if (ctrl->new_medium[ctrl->unit]) {
      if (!ctrl->mask[ctrl->unit].power_fail)
	ctrl->err[ctrl->unit].power_fail = 1; 
      ctrl->new_medium[ctrl->unit] = 0;
    }
    // check for disk
    if (ctrl->disk[ctrl->unit] == NULL) {
      if (!ctrl->mask[ctrl->unit].not_ready)
	ctrl->err[ctrl->unit].not_ready = 1;
    } else {
      ctrl->addr[ctrl->unit] = ctrl->address[ctrl->unit] * ctrl->nbsector[ctrl->unit];
      if (ctrl->length[ctrl->unit] != 0)
	ctrl->rwvd = 2;		// do a write on next execution
    }
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 0;
    break;

    // MLA & secondary 05 , op 0x04 : Locate and verify
  case 3200:
    // check for new medium
    if (ctrl->new_medium[ctrl->unit]) {
      if (!ctrl->mask[ctrl->unit].power_fail)
	ctrl->err[ctrl->unit].power_fail = 1; 
      ctrl->new_medium[ctrl->unit] = 0;
    }
    _ASSERT(0);
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 0;
    break;

    // MLA & secondary 05 , op 0x06 : Spare block
  case 3300:
    // check for new medium
    if (ctrl->new_medium[ctrl->unit]) {
      if (!ctrl->mask[ctrl->unit].power_fail)
	ctrl->err[ctrl->unit].power_fail = 1; 
      ctrl->new_medium[ctrl->unit] = 0;
    }
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      if (!ctrl->mask[ctrl->unit].no_spare_available)
	ctrl->err[ctrl->unit].no_spare_available = 1; 
      ctrl->stss80 = 0;
    }
    break;

    // MLA & secondary 05 , op 0x0D : Request status
  case 3400:
    ctrl->data[0] = (ctrl->volume << 4) | (ctrl->unit);
    ctrl->data[1] = 0xFF;
    ctrl->data[2] = ctrl->err[ctrl->unit].status[0];
    ctrl->data[3] = ctrl->err[ctrl->unit].status[1];
    ctrl->data[4] = ctrl->err[ctrl->unit].status[2];
    ctrl->data[5] = ctrl->err[ctrl->unit].status[3];
    ctrl->data[6] = ctrl->err[ctrl->unit].status[4];
    ctrl->data[7] = ctrl->err[ctrl->unit].status[5];
    ctrl->data[8] = ctrl->err[ctrl->unit].status[6];
    ctrl->data[9] = ctrl->err[ctrl->unit].status[7];
    if (ctrl->err[ctrl->unit].cross_unit) {	// do P1-P6 for cross-unit errors (can only append for tapes!)
      ctrl->data[10] = 0x00;
      ctrl->data[11] = 0x00;
      ctrl->data[12] = 0x00;
      ctrl->data[13] = 0x00;
      ctrl->data[14] = 0x00;
      ctrl->data[15] = 0x00;
    } else if (ctrl->err[ctrl->unit].diagnostic_result) {		// do P1-P6 for diagnostic result
      ctrl->data[10] = 0x00;
      ctrl->data[11] = 0x00;
      ctrl->data[12] = 0x00;
      ctrl->data[13] = 0x00;
      ctrl->data[14] = 0x00;
      ctrl->data[15] = ctrl->unit;	// failed unit
    } else if (ctrl->err[ctrl->unit].unrecoverable_data) {		// do P1-P6 for unrecoverable data
      ctrl->data[10] = (BYTE) (ctrl->addressh[ctrl->unit] >> 8);
      ctrl->data[11] = (BYTE) (ctrl->addressh[ctrl->unit] & 0xFF);
      ctrl->data[12] = (BYTE) (ctrl->address[ctrl->unit] >> 24);
      ctrl->data[13] = (BYTE) (ctrl->address[ctrl->unit] >> 16);
      ctrl->data[14] = (BYTE) (ctrl->address[ctrl->unit] >> 8);
      ctrl->data[15] = (BYTE) (ctrl->address[ctrl->unit] & 0xFF);
    } else if (ctrl->err[ctrl->unit].recoverable_data) {		// do P1-P6 for recoverable data
      ctrl->data[10] = (BYTE) (ctrl->addressh[ctrl->unit] >> 8);
      ctrl->data[11] = (BYTE) (ctrl->addressh[ctrl->unit] & 0xFF);
      ctrl->data[12] = (BYTE) (ctrl->address[ctrl->unit] >> 24);
      ctrl->data[13] = (BYTE) (ctrl->address[ctrl->unit] >> 16);
      ctrl->data[14] = (BYTE) (ctrl->address[ctrl->unit] >> 8);
      ctrl->data[15] = (BYTE) (ctrl->address[ctrl->unit] & 0xFF);
    } else {								// P1-P6 for no error
      if (ctrl->unit == 15) {						// for unit 15
	ctrl->data[10] = 0x00;
	ctrl->data[11] = 0x00;
	ctrl->data[12] = 0x00;
	ctrl->data[13] = 0x00;
	ctrl->data[14] = 0x00;
	ctrl->data[15] = 0x00;
      } else {
	ctrl->data[10] = (BYTE) (ctrl->addressh[ctrl->unit] >> 8);
	ctrl->data[11] = (BYTE) (ctrl->addressh[ctrl->unit] & 0xFF);
	ctrl->data[12] = (BYTE) (ctrl->address[ctrl->unit] >> 24);
	ctrl->data[13] = (BYTE) (ctrl->address[ctrl->unit] >> 16);
	ctrl->data[14] = (BYTE) (ctrl->address[ctrl->unit] >> 8);
	ctrl->data[15] = (BYTE) (ctrl->address[ctrl->unit] & 0xFF);
      }
    }
    ctrl->data[16] = 0x00;
    ctrl->data[17] = 0x00;
    ctrl->data[18] = 0x00;
    ctrl->data[19] = 0x00;
    ctrl->stss80++;
    break;
  case 3401:		// now clear the status
    ctrl->err[ctrl->unit].status[0] = 0x00;
    ctrl->err[ctrl->unit].status[1] = 0x00;
    ctrl->err[ctrl->unit].status[2] = 0x00;
    ctrl->err[ctrl->unit].status[3] = 0x00;
    ctrl->err[ctrl->unit].status[4] = 0x00;
    ctrl->err[ctrl->unit].status[5] = 0x00;
    ctrl->err[ctrl->unit].status[6] = 0x00;
    ctrl->err[ctrl->unit].status[7] = 0x00;
    ctrl->dcount = 20;
    ctrl->rwvd = 4;			// data execution mode
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;		// loop back as complementary
    break;

    // MLA & secondary 05 , op 0x10 : Set address
  case 3500:
    if (ctrl->unit > 1) 
      if (!ctrl->mask[ctrl->unit].illegal_parameter)
	ctrl->err[ctrl->unit].illegal_parameter = 1;
    ctrl->stss80++;
  case 3501:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      if (ctrl->count < 4)
	ctrl->dword |= (ctrl->c << (ctrl->count << 3));
      else 
	ctrl->word |= (ctrl->c << ((ctrl->count-4) << 3));
      ctrl->count--;
      if (ctrl->count == 0xFFFF) {
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 10 Set address : %04X%08X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->addressh[ctrl->unit], ctrl->address[ctrl->unit]);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	if (ctrl->qstat[ctrl->unit] == 0) {
	  if ((ctrl->word != 00) || (ctrl->dword >= ctrl->totalsectors[ctrl->unit])) {
	    if (!ctrl->mask[ctrl->unit].address_bounds)
	      ctrl->err[ctrl->unit].address_bounds = 1;
	  } else {
	    ctrl->addressh[ctrl->unit] = ctrl->word;
	    ctrl->address[ctrl->unit] = ctrl->dword;
	  }
	}
	ctrl->stss80 = 1000;			// loop back
      }
    }
    break;

    // MLA & secondary 05 , op 0x18 : complementary : Set Length
  case 3600:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      ctrl->dword |= (ctrl->c << (ctrl->count << 3));
      ctrl->count--;
      if (ctrl->count == 0xFFFF) {
	if (ctrl->qstat[ctrl->unit] == 0) {
	  ctrl->length[ctrl->unit] = ctrl->dword;
	  if (ctrl->dword == 0xFFFFFFF)
	    ctrl->length[ctrl->unit] = ctrl->totalsectors[ctrl->unit] * ctrl->nbsector[ctrl->unit];
	}
#if defined DEBUG_HP9122
	k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 18 Set length : %08X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->length[ctrl->unit]);
	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	ctrl->stss80 = 1000;			// loop back
      }
    }
    break;

    // MLA & secondary 05 , op 0x31 : Initiate utility : validate, format option (F3, 5F), , download : not done
  case 3700:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// got first byte
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122 : MLA SEC5: OP 31 ... %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      if (ctrl->c == 0xF1)
	ctrl->stss80 = 3790;		// validate key ?
      else if (ctrl->c == 0xF2)	
	ctrl->stss80 = 3780;		// download ?
      else if (ctrl->c == 0xF3)
	ctrl->stss80 = 3710;		// format option ?
      else {
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 1000;			// restart
      }
    }
    break;

    // MLA & secondary 05 , op 0x31, 0xF3, 0x5F : format option	: 0x00 : default, 0xFF : parameter bounds error if no option else, option
    //		for a 9122, 0 : 256 bytes/blocks 2 sides,
    //					1 : 256		     2
    //					2 : 512				 2
    //					3 :1024				 2
    //					4 : 256				 1 side (9121 compatible)
  case 3710:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// got second byte
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31 ...    %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      if (ctrl->c == 0x5F) {
	ctrl->rwvd = 5;				// get data byte and go back
      }
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;				// restart
    }
    break;
  case 3711:					// got byte ... continue
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31 format option : %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->data[0]);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    switch (ctrl->data[0]) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      ctrl->ftype[ctrl->unit] = ctrl->data[0];
      break;
    case 0xFF:				// no error there is a parameter possible ...
      break;
    default:
      if (!ctrl->mask[ctrl->unit].parameter_bounds)
	ctrl->err[ctrl->unit].parameter_bounds = 1;				
      break;
    }
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;				// restart
    break;

    // MLA & secondary 05 , op 0x31, 0xF2, 0xA5 : download
  case 3780:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// got second byte
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31 ...    %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      if (ctrl->c == 0xA5)
	ctrl->stss80++;
      else {
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 1000;				// restart
      }
    }
    break;
  case 3781:
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31	    Download\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    _ASSERT(0);
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;				// restart
    break;

    // MLA & secondary 05 , op 0x31, 0xF1, 0x02 : validate key
  case 3790:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// got second byte
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31 ...    %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      if (ctrl->c == 0x02)
	ctrl->stss80++;
      else {
	ctrl->ppol_e = TRUE;
	ctrl->stss80 = 1000;				// restart
      }
    }
    break;
  case 3791:
#if defined DEBUG_HP9122
    k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 31	    Validate key\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    _ASSERT(0);
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;				// restart
    break;

    // MLA & secondary 05 , op 0x33 : Initiate diagnosis
  case 3800:
    _ASSERT(0);
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;				// restart
    break;

    // MLA & secondary 05 , op 0x35 : complementary : Describe
  case 3900:
    // check for new medium
    if (ctrl->new_medium[ctrl->unit]) {
      if (!ctrl->mask[ctrl->unit].power_fail)
	ctrl->err[ctrl->unit].power_fail = 1; 
      ctrl->new_medium[ctrl->unit] = 0;
    }
    if (ctrl->disk[ctrl->unit] == NULL) {
      if (!ctrl->mask[ctrl->unit].not_ready)
	ctrl->err[ctrl->unit].not_ready = 1;
    }
    // controller description
    ctrl->data[0] = 0x00;				// no unit 15			-40
    ctrl->data[1] = 0x03;				// units 1,0			-39
    ctrl->data[2] = 0x00;				//				-38
    ctrl->data[3] = 0x64;				// 100 000 bytes/seconds	-37
    ctrl->data[4] = 0x05;				// ss/80 multi-unit controler	-36
    ctrl->count = 5;
    if (ctrl->unit == 15) 
      ctrl->stss80 = 3920;
    else {
      ctrl->dcount = 0;
      ctrl->stss80 = 3910;
    }
    break;
  case 3910:						// do a unit
    // unit description
    ctrl->data[ctrl->count++] = 0x01;			// flexible or removable disk		-35
    ctrl->data[ctrl->count++] = 0x09;
    ctrl->data[ctrl->count++] = 0x12;			//					-33
    ctrl->data[ctrl->count++] = 0x20;			// device number: 9122 option 0	
    dw = (ctrl->disk[ctrl->unit] == NULL) ? 256 : ctrl->nbsector[ctrl->unit];
    ctrl->data[ctrl->count++] = (BYTE) (dw >> 8);	//					-31
    ctrl->data[ctrl->count++] = (BYTE) (dw & 0xFF);	// bytes per block
    ctrl->data[ctrl->count++] = 1;			// 1 block can be buffered		-29
    ctrl->data[ctrl->count++] = 0;			// recommended burst size 0 for SS/80	
    ctrl->data[ctrl->count++] = 0x00;			//					-27
    ctrl->data[ctrl->count++] = 0x10;			// 16 µs block time
    ctrl->data[ctrl->count++] = 0x00;			//					-25
    ctrl->data[ctrl->count++] = 0x2D;			// 45 000 bytes/seconde continuous average transfert rate
    ctrl->data[ctrl->count++] = 0x11;			//					-23
    ctrl->data[ctrl->count++] = 0x94;			// optimal retry is 45 sec
    ctrl->data[ctrl->count++] = 0x20;			//					-21
    ctrl->data[ctrl->count++] = 0xD0;			// acces time parameter is 84 sec
    ctrl->data[ctrl->count++] = 1;			// maximum interleave factor		-19
    ctrl->data[ctrl->count++] = 0x00;			// fixed volume byte : no fixed volume
    //ctrl->data[ctrl->count++] = (ctrl->disk[ctrl->unit] == NULL) ? 0x00 : 0x01; // removable volume byte : no removable volume
    ctrl->data[ctrl->count++] = 0x01;			// removable volume byte : one volume	-17
    // volume description
    if (ctrl->disk[ctrl->unit] == NULL) {
      ctrl->data[ctrl->count++] = 0;							//	-16
      ctrl->data[ctrl->count++] = 0;							//	-15
      ctrl->data[ctrl->count++] = 0;							//	-14
      ctrl->data[ctrl->count++] = 0;							//	-13
      ctrl->data[ctrl->count++] = 0;							//	-12
      ctrl->data[ctrl->count++] = 0;							//	-11
      dw = 0;
    } else {
      ctrl->data[ctrl->count++] = (BYTE) (((ctrl->ncylinders[ctrl->unit] - 1) & 0xFF0000) >> 16);//	-16
      ctrl->data[ctrl->count++] = (BYTE) (((ctrl->ncylinders[ctrl->unit] - 1) & 0xFF00) >> 8);	 //	-15
      ctrl->data[ctrl->count++] = (BYTE) ((ctrl->ncylinders[ctrl->unit] - 1)& 0xFF);		 //	-14
      ctrl->data[ctrl->count++] = (BYTE) (ctrl->nheads[ctrl->unit] - 1);			 //	-13
      ctrl->data[ctrl->count++] = (BYTE) (((ctrl->nsectors[ctrl->unit] - 1) & 0xFF00) >> 8);	 //	-12
      ctrl->data[ctrl->count++] = (BYTE) ((ctrl->nsectors[ctrl->unit] - 1) & 0xFF);		 //	-11
      dw = ctrl->ncylinders[ctrl->unit] * 
	ctrl->nheads[ctrl->unit] * 
	ctrl->nsectors[ctrl->unit] - 1;
    }
    ctrl->data[ctrl->count++] = 0;					//			-10
    ctrl->data[ctrl->count++] = 0;					//			- 9
    ctrl->data[ctrl->count++] = (BYTE) ((dw & 0xFF000000) >> 24);	//			- 8
    ctrl->data[ctrl->count++] = (BYTE) ((dw & 0xFF0000) >> 16);		//			- 7
    ctrl->data[ctrl->count++] = (BYTE) ((dw & 0xFF00) >> 8);		//			- 6
    ctrl->data[ctrl->count++] = (BYTE) (dw & 0xFF);			//			- 5
    ctrl->data[ctrl->count++] = 1;					// interleave factor .. - 4

    if (ctrl->dcount == 0)
      ctrl->stss80 = 3930;						// only one unit, leave
    else 
      ctrl->stss80 = (WORD) ctrl->dcount;				// jump back for the nex unit
    break;

  case 3920:
    ctrl->unit = 0;
    ctrl->dcount = 3921;
    ctrl->stss80 = 3910;
    break;
  case 9321:
    ctrl->unit = 1;
    ctrl->dcount = 3922;
    ctrl->stss80 = 3910;
    break;
  case 9322:
    ctrl->unit = 15;
    ctrl->stss80 = 3930;
    break;
  case 3930:
    ctrl->dcount = ctrl->count;
    ctrl->rwvd = 4;			// data execution mode
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;		// loop back as complementary
    break;

    // MLA & secondary 05 , op 0x37 : Initialize media
  case 4000:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {		// first byte	: spare and all no use here
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 37 ... %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      ctrl->stss80++;
    }
    break;
  case 4001:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// got second byte	: interleave, no use here
#if defined DEBUG_HP9122
      k = wsprintf(buffer,_T("%06X: HP9122:%d: MLA SEC5: OP 37 ...    %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
      ctrl->stss80++;
    }
    break;
  case 4002:		// all done (rather quick :)
    if (ctrl->type[ctrl->unit] != ctrl->ftype[ctrl->unit]) {		// different format ...
      switch(ctrl->ftype[ctrl->unit]) {
      default:
	ctrl->nbsector[ctrl->unit] = 256;
	ctrl->nsectors[ctrl->unit] = 16;
	ctrl->ncylinders[ctrl->unit] = 77;
	ctrl->nheads[ctrl->unit] = 2;
	break;
      case 1:
	ctrl->nbsector[ctrl->unit] = 256;
	ctrl->nsectors[ctrl->unit] = 16;
	ctrl->ncylinders[ctrl->unit] = 77;
	ctrl->nheads[ctrl->unit] = 2;
	break;
      case 2:
	ctrl->nbsector[ctrl->unit] = 512;
	ctrl->nsectors[ctrl->unit] = 9;
	ctrl->ncylinders[ctrl->unit] = 77;
	ctrl->nheads[ctrl->unit] = 2;
	break;
      case 3:
	ctrl->nbsector[ctrl->unit] = 1024;
	ctrl->nsectors[ctrl->unit] = 5;
	ctrl->ncylinders[ctrl->unit] = 77;
	ctrl->nheads[ctrl->unit] = 2;
      case 4:
	ctrl->nbsector[ctrl->unit] = 256;
	ctrl->nsectors[ctrl->unit] = 16;
	ctrl->ncylinders[ctrl->unit] = 33;
	ctrl->nheads[ctrl->unit] = 2;
      }
      ctrl->type[ctrl->unit] = ctrl->ftype[ctrl->unit];
      free(ctrl->disk[ctrl->unit]);
      ctrl->disk[ctrl->unit] = malloc(disk_bytes[ctrl->type[ctrl->unit]]);
      bzero( ctrl->disk[ctrl->unit],disk_bytes[ctrl->type[ctrl->unit]]);
    }
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 1000;					// loop back as complementary
    break;

    // MLA & secondary 05 , op 0x3E : complementary : Set status mask
  case 4100:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      ctrl->mask[ctrl->unit].status[ctrl->count++] = ctrl->c;
      if (ctrl->count == 8) {
	if ((ctrl->mask[ctrl->unit].status[2] != 0x00) || (ctrl->mask[ctrl->unit].status[3] != 0x00)) {
	  ctrl->mask[ctrl->unit].status[2] = 0x00;
	  ctrl->mask[ctrl->unit].status[3] = 0x00;
	}
	ctrl->ppol_e = TRUE;	// needed for 9836
	ctrl->stss80 = 1000;	// loop back as complementary
      }
    }
    break;

    // MLA & secondary 05 , op 0x48 : complementary : Set return addressing mode
  case 4200:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      if (ctrl->c != 0) {
	if (!ctrl->mask[ctrl->unit].parameter_bounds)
	  ctrl->err[ctrl->unit].parameter_bounds = 1;
      }
      ctrl->stss80 = 1000;		// loop back as complementary
    }
    break;

    // for no op, skip count bytes
  case 4300:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {
      ctrl->count--;
      if (ctrl->count == 0) {
	ctrl->stss80 = 1000;		// loop back as complementary
      }
    }
    break;

    // MLA & secondary 12 , op 0x01 : HP-IB Parity checking
  case 5000:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// get data byte
      ctrl->stss80++;			// restart, do nothing
    }
    break;
  case 5001:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {	// should be UNL
      // ctrl->ppol_e = TRUE;		// no ppe after
      ctrl->stss80 = 2;			// restart
    }
    break;

    // MLA & secondary 12 , op 0x02 : Read Loopback	For testing transmission medium
  case 5100:
    _ASSERT(0);
    break;

    // MLA & secondary 12 , op 0x03 : Write Loopback	For testing transmission medium
  case 5200:
    _ASSERT(0);
    break;

    // MLA & secondary 12 , op 0x08 : Channel independant clear
  case 5300:	// clear designated unit
    if (!ctrl->listen)		// should be UNL
      ctrl->stss80++;
    break;
  case 5301:
    ctrl->volume = 0;
    ctrl->addressh[ctrl->unit] = 0;
    ctrl->address[ctrl->unit] = 0;
    ctrl->length[ctrl->unit] = 0xFFFFFFFF;
    ctrl->mask[ctrl->unit].status[0] = 0;
    ctrl->mask[ctrl->unit].status[1] = 0;
    ctrl->mask[ctrl->unit].status[2] = 0;
    ctrl->mask[ctrl->unit].status[3] = 0;
    ctrl->mask[ctrl->unit].status[4] = 0;
    ctrl->mask[ctrl->unit].status[5] = 0;
    ctrl->mask[ctrl->unit].status[6] = 0;
    ctrl->mask[ctrl->unit].status[7] = 0;
    ctrl->err[ctrl->unit].status[0] = 0;
    ctrl->err[ctrl->unit].status[1] = 0;
    ctrl->err[ctrl->unit].status[2] = 0;
    ctrl->err[ctrl->unit].status[3] = 0;
    ctrl->err[ctrl->unit].status[4] = 0;
    ctrl->err[ctrl->unit].status[5] = 0;
    ctrl->err[ctrl->unit].status[6] = 0;
    ctrl->err[ctrl->unit].status[7] = 0;
    if (ctrl->unit == 15) 
      ctrl->stss80++;
    else {
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 0;				// restart
    }
    break;
  case 5302:	// clear unit 1
    ctrl->unit = 1;
    ctrl->addressh[ctrl->unit] = 0;
    ctrl->address[ctrl->unit] = 0;
    ctrl->length[ctrl->unit] = 0xFFFFFFFF;
    ctrl->mask[ctrl->unit].status[0] = 0;
    ctrl->mask[ctrl->unit].status[1] = 0;
    ctrl->mask[ctrl->unit].status[2] = 0;
    ctrl->mask[ctrl->unit].status[3] = 0;
    ctrl->mask[ctrl->unit].status[4] = 0;
    ctrl->mask[ctrl->unit].status[5] = 0;
    ctrl->mask[ctrl->unit].status[6] = 0;
    ctrl->mask[ctrl->unit].status[7] = 0;
    ctrl->err[ctrl->unit].status[0] = 0;
    ctrl->err[ctrl->unit].status[1] = 0;
    ctrl->err[ctrl->unit].status[2] = 0;
    ctrl->err[ctrl->unit].status[3] = 0;
    ctrl->err[ctrl->unit].status[4] = 0;
    ctrl->err[ctrl->unit].status[5] = 0;
    ctrl->err[ctrl->unit].status[6] = 0;
    ctrl->err[ctrl->unit].status[7] = 0;
    ctrl->stss80++;
    break;
  case 5303:	// clear unit 0
    ctrl->unit = 0;
    ctrl->addressh[ctrl->unit] = 0;
    ctrl->address[ctrl->unit] = 0;
    ctrl->length[ctrl->unit] = 0xFFFFFFFF;
    ctrl->mask[ctrl->unit].status[0] = 0;
    ctrl->mask[ctrl->unit].status[1] = 0;
    ctrl->mask[ctrl->unit].status[2] = 0;
    ctrl->mask[ctrl->unit].status[3] = 0;
    ctrl->mask[ctrl->unit].status[4] = 0;
    ctrl->mask[ctrl->unit].status[5] = 0;
    ctrl->mask[ctrl->unit].status[6] = 0;
    ctrl->mask[ctrl->unit].status[7] = 0;
    ctrl->err[ctrl->unit].status[0] = 0;
    ctrl->err[ctrl->unit].status[1] = 0;
    ctrl->err[ctrl->unit].status[2] = 0;
    ctrl->err[ctrl->unit].status[3] = 0;
    ctrl->err[ctrl->unit].status[4] = 0;
    ctrl->err[ctrl->unit].status[5] = 0;
    ctrl->err[ctrl->unit].status[6] = 0;
    ctrl->err[ctrl->unit].status[7] = 0;
    ctrl->ppol_e = TRUE;
    ctrl->stss80 = 0;				// restart
    break;

    // MLA & secondary 12 , op 0x09 : Cancel, here do nothing
  case 5400:
    if (pop_d(ctrl, &ctrl->c, &eoi)) {		// wait for UNL
      _ASSERT(0);
      ctrl->ppol_e = TRUE;
      ctrl->stss80 = 2;				// restart with command
    }
    break;
  }

  // check for QSTAT
  if ((ctrl->err[ctrl->unit].status[0] != 0x00) ||
      (ctrl->err[ctrl->unit].status[1] != 0x00) ||
      (ctrl->err[ctrl->unit].status[2] != 0x00) ||
      (ctrl->err[ctrl->unit].status[3] != 0x00) ||
      (ctrl->err[ctrl->unit].status[4] != 0x00) ||
      (ctrl->err[ctrl->unit].status[5] != 0x00) ||
      (ctrl->err[ctrl->unit].status[6] != 0x00) ||
      (ctrl->err[ctrl->unit].status[7] != 0x00)) 
    ctrl->qstat[ctrl->unit] = 1;
  else ctrl->qstat[ctrl->unit] = 0;
  if (ctrl->err[ctrl->unit].power_fail) ctrl->qstat[ctrl->unit] = 2;
	
}

//
// load a volume in unit in ram
//
BOOL hp9122_load(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename) {
  INT	hDiskFile = -1;
  DWORD dwFileSize, dwRead;
  LPBYTE pDisk = NULL;
  struct stat rs;
  hDiskFile = open(szFilename,O_EXCL,O_RDWR);
  if (hDiskFile < 0)  {
    perror("hp9122 image");
    hDiskFile = -1;
    return FALSE;
  }
  fstat(hDiskFile,&rs);
  dwFileSize = rs.st_size;

  if (dwFileSize == disk_bytes[0]) {			// type 0
    ctrl->type[unit] = 0;
    ctrl->nbsector[unit] = 256;
    ctrl->nsectors[unit] = 16;
    ctrl->ncylinders[unit] = 77;
    ctrl->nheads[unit] = 2;
  } else if (dwFileSize == disk_bytes[1]) {		// type 1
    ctrl->type[unit] = 1;
    ctrl->nbsector[unit] = 256;
    ctrl->nsectors[unit] = 16;
    ctrl->ncylinders[unit] = 77;
    ctrl->nheads[unit] = 2;
  } else if (dwFileSize == disk_bytes[2]) {		// type 2
    ctrl->type[unit] = 2;
    ctrl->nbsector[unit] = 512;
    ctrl->nsectors[unit] = 9;
    ctrl->ncylinders[unit] = 77;
    ctrl->nheads[unit] = 2;
  } else if (dwFileSize == disk_bytes[3]) {		// type 3
    ctrl->type[unit] = 3;
    ctrl->nbsector[unit] = 1024;
    ctrl->nsectors[unit] = 5;
    ctrl->ncylinders[unit] = 77;
    ctrl->nheads[unit] = 2;
  } else if (dwFileSize == disk_bytes[4]) {		// type 4
    ctrl->type[unit] = 4;
    ctrl->nbsector[unit] = 256;
    ctrl->nsectors[unit] = 16;
    ctrl->ncylinders[unit] = 33;
    ctrl->nheads[unit] = 2; 
  } else {						// type unknown
    close(hDiskFile);
    return FALSE;
  }
  ctrl->totalsectors[unit] = ctrl->nheads[unit] * ctrl->nsectors[unit] * ctrl->ncylinders[unit];

  pDisk = malloc(disk_bytes[ctrl->type[unit]]);
  if (pDisk == NULL)	{
      close(hDiskFile);
      return FALSE;
    }

  // load file content)
  dwRead = read(hDiskFile, pDisk, dwFileSize);
  close(hDiskFile);
  _ASSERT(dwFileSize == dwRead);

  if (ctrl->disk[unit] != NULL)	 free(ctrl->disk[unit]);
  ctrl->disk[unit] = pDisk;

  ctrl->new_medium[unit] = 1;					// disk changed

  strcpy(ctrl->name[unit], szFilename);

  GetLifName(ctrl, unit);
  emuUpdateButton(ctrl->hpibaddr, unit, ctrl->lifname[unit]);

  return TRUE;
}

// save a volume in unit from ram
BOOL hp9122_save(HPSS80 *ctrl, BYTE unit, LPCTSTR szFilename) {
  INT	hDiskFile = -1;
  DWORD dwWritten;

  if (ctrl->disk[unit] == NULL)	 return FALSE;

  hDiskFile = creat(szFilename,O_WRONLY);
  if (hDiskFile <0)	{
    hDiskFile = -1;
    return FALSE;
  }    

  // save file content
  dwWritten = write(hDiskFile, ctrl->disk[unit], disk_bytes[ctrl->type[unit]]);
  close(hDiskFile);
  strcpy(ctrl->name[unit], szFilename);

  _ASSERT((DWORD)(disk_bytes[ctrl->type[unit]]) == dwWritten);

  return TRUE;
}

// 
// eject a volume in unit NO SAVE all changes lost
//
BOOL hp9122_eject(HPSS80 *ctrl, BYTE unit) {
  if (ctrl->disk[unit] != NULL)
    free(ctrl->disk[unit]);
  ctrl->name[unit][0] = 0x00;
  ctrl->disk[unit] = NULL;

  //	ctrl->err[unit].power_fail = 1;				// disk changed

  ctrl->lifname[unit][0] = 0x00;
  emuUpdateButton(ctrl->hpibaddr, unit, ctrl->lifname[unit]);

  return TRUE;
}

//
// wait for idle state of controller (not really used)
//
BOOL hp9122_widle(HPSS80 *ctrl) {
  DWORD dwRefTime;

  dwRefTime = timeGetTime();
  //	Chipset.idle = FALSE;
  usleep(10000);
  // wait for the Iddle state with 5 sec timeout
  while (((timeGetTime() - dwRefTime) < 5000L) && !(ctrl->stss80 < 2))
    usleep(10000);

  if (ctrl->stss80 < 2)						// not timeout, HPSS80 is idle
    return FALSE;						// idle state
  return TRUE;							// HPSS80 was busy
}

//
// reset the instrument
//
VOID hp9122_reset(VOID *controler) {
  HPSS80 *ctrl = (HPSS80 *) controler;
  BYTE i;

  for (i = 0; i < 2; i++) {
    if (ctrl->name[i][0] != 0x00) {
      hp9122_load(ctrl, i, ctrl->name[i]);
    }
    if (ctrl->name[i][0] == 0x00) {
      emuUpdateButton(ctrl->hpibaddr, ctrl->unit, "");
    }

    ctrl->head[i] = 0;
    ctrl->cylinder[i] = 0;
    ctrl->sector[i] = 0;
    ctrl->addr[i] = 0;

    ctrl->addressh[i] = 0;
    ctrl->address[i] = 0;
    ctrl->length[i] = 0xFFFFFFFF;
    ctrl->mask[i].status[0] = 0;
    ctrl->mask[i].status[1] = 0;
    ctrl->mask[i].status[2] = 0;
    ctrl->mask[i].status[3] = 0;
    ctrl->mask[i].status[4] = 0;
    ctrl->mask[i].status[5] = 0;
    ctrl->mask[i].status[6] = 0;
    ctrl->mask[i].status[7] = 0;
    ctrl->err[i].status[0] = 0;
    ctrl->err[i].status[1] = 0;
    ctrl->err[i].status[2] = 0;
    ctrl->err[i].status[3] = 0;
    ctrl->err[i].status[4] = 0;
    ctrl->err[i].status[5] = 0;
    ctrl->err[i].status[6] = 0;
    ctrl->err[i].status[7] = 0;
    ctrl->err[i].power_fail = 1;				// disk changed
    ctrl->qstat[i] = 2;
  }
  i = 15;
  ctrl->mask[i].status[0] = 0;
  ctrl->mask[i].status[1] = 0;
  ctrl->mask[i].status[2] = 0;
  ctrl->mask[i].status[3] = 0;
  ctrl->mask[i].status[4] = 0;
  ctrl->mask[i].status[5] = 0;
  ctrl->mask[i].status[6] = 0;
  ctrl->mask[i].status[7] = 0;
  ctrl->err[i].status[0] = 0;
  ctrl->err[i].status[1] = 0;
  ctrl->err[i].status[2] = 0;
  ctrl->err[i].status[3] = 0;
  ctrl->err[i].status[4] = 0;
  ctrl->err[i].status[5] = 0;
  ctrl->err[i].status[6] = 0;
  ctrl->err[i].status[7] = 0;
  ctrl->err[i].power_fail = 1;					// power-up
  ctrl->qstat[i] = 2;

  ctrl->unit = 0;
  ctrl->volume = 0;

  ctrl->stss80 = 0;						// state of HPSS80 controller
  ctrl->untalk = FALSE;						// previous command was UNTALK ?
  ctrl->talk = FALSE;						// MTA received ?
  ctrl->listen = FALSE;						// MLA received ?
  ctrl->ppol_e = TRUE;						// parallel poll enabled
  ctrl->hc_hi = 0;						// hi mark
  ctrl->hc_lo = 0;						// lo mark
  ctrl->hd_hi = 0;						// hi mark
  ctrl->hd_lo = 0;						// lo mark
}

//
// stop the disks for saving ...
//
VOID hp9122_stop(VOID *controler) {
  HPSS80 *ctrl = (HPSS80 *) controler;
  int unit;

  for (unit = 0; unit < 2; unit++) {
    if (ctrl->disk[unit] != NULL)
      free(ctrl->disk[unit]);
    ctrl->disk[unit] = NULL;

    ctrl->lifname[unit][0] = 0x00;
    emuUpdateButton(ctrl->hpibaddr, unit, ctrl->lifname[unit]);
  }
}
