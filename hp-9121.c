/*
 *   Hp-9121.c
 *
 *   Copyright 2004-2011 Olivier De Smet
 */

//
//   HP9121 floppy disk as ADDRESS 0 simulation based on behaviors from
//
//   "HP flexible disctrl->c drive command set Appendix A" from an HP9121 manual, amigo protocol
//   hpfddcs.pdf found on the WWW
//   "Original 320 rom listing from HP"
//   ROM320_P1.pdf from André Koppel
//
//   emulate 9121 double unit, 9895A double unit (and 9134 as 1 huge unit) 
//      type   0,               1,                  2 

#include "common.h"
#include "hp9816emu.h"
#include "kml.h"

#define DELAY_CMD 50				// delay for command response

//#define DEBUG_HP9121				// debug flag

#if defined DEBUG_HP9121
static WORD addr[] = {				// states of automata
  0, 1000, 1100, 1400, 
  2000,
  9000,
  10000, 10500, 11000, 11200, 11500,
  12000, 12500, 13000, 13500,
  14000, 14100, 14500, 14700, 15000, 15500,
  16000, 16500, 17000, 17500,
  18000, 18500, 19000, 
  20000, 20500, 21000, 21500,
  22000, 22500
};

static TCHAR *HP9121_TXT[] ={				// labels of states of automata 
  _T("Idle"), _T("MSA received"), _T("MLA received"), _T("MTA received"),
  _T("Identify"),
  _T("Get 1 byte"),
  _T("Receive data"), _T("Cold load read"), _T("Seek"), _T("Set Address Record"), _T("Request ctrl->Status"),
  _T("Verify"), _T("Initialize"), _T("Request logical address"), _T("End"),
  _T("Buffered write"), _T("Unbuffered write"), _T("Buffered read"), _T("Unbuffered read"), _T("Unbuffered read verify"), _T("Request physical address"),
  _T("Send wear"), _T("Format"), _T("Download"), _T("HP-300 clear"),
  _T("HP-IB-CRC L"), _T("Write loopback record"), _T("Initiate selftest"), 
  _T("Send data"), _T("Send ctrl->Status or address"), _T("DSJ"), _T("HP-IB-CRC T"),
  _T("Read loopback record"), _T("Read selftest")
};

#define NLAB (sizeof(addr)/sizeof(addr[0]))

static TCHAR *HP9121_LAB(WORD d)			// find a label for the state d
{
  WORD i;

  for(i = 0; i < NLAB; i++)
    if (addr[i] == d)
      return HP9121_TXT[i];
  return NULL;
}
#endif

// hp9121 variables

#define DISK_BYTES ((ctrl->config_heads * ctrl->config_cylinders * ctrl->config_sectors) << 8)
// 0x01,0x04 ->    2,       33,     32
// 0x00,0x81 ->    2,       75,     60
// 0x01,0x06 ->    4,      153,    124
//              is 4 unit of 2, 306, 31
//              head, cylinder, sect/cylinder (include head)
//################
//#
//#    Low level subroutines
//#
//################

#include <sys/time.h>

long timeGetTime() {
  struct timeval tv;
  long res;
  gettimeofday(&tv,NULL);
  res = tv.tv_sec*1000+tv.tv_usec/1000;
  return res;
}

static VOID GetLifName(HP9121 *ctrl, BYTE unit) {
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

static VOID raj_addr(HP9121 *ctrl, BYTE u) {
  while (ctrl->sector[u] >= ctrl->config_sectors)
    ctrl->sector[u] -= ctrl->config_sectors;
  while (ctrl->head[u] >= ctrl->config_heads)
    ctrl->head[u] -= ctrl->config_heads;
  while (ctrl->cylinder[u] >= ctrl->config_cylinders)
    ctrl->cylinder[u] -= ctrl->config_cylinders;
  ctrl->addr[u] = (ctrl->sector[u] + 
		   ctrl->head[u] * ctrl->config_sectors + 
		   ctrl->cylinder[u] * ctrl->config_heads * ctrl->config_sectors
		   ) * 256;
}

static VOID inc_addr(HP9121 *ctrl, BYTE u) {
  ctrl->sector[u]++;
  if (ctrl->sector[u] >= ctrl->config_sectors)  {
    ctrl->sector[u] = 0;
    ctrl->head[u]++;
    if (ctrl->head[u] >= ctrl->config_heads)	{
      ctrl->head[u] = 0;
      ctrl->cylinder[u]++;
      if (ctrl->cylinder[u] >= ctrl->config_cylinders)
	ctrl->cylinder[u] = 0;	// wrap
    }
  }
  raj_addr(ctrl, u);
}

static BOOL pop_c(HP9121 *ctrl, BYTE *c) {
  if (ctrl->hc_hi == ctrl->hc_lo)
    return FALSE;
  if (ctrl->hc_t[ctrl->hc_lo] != 0) {		// wait more
    ctrl->hc_t[ctrl->hc_lo]--;
    return FALSE;
  }
  *c = ctrl->hc[ctrl->hc_lo++];
  ctrl->hc_lo &= 0x1FF;
  return TRUE;
}

static BOOL pop_d(HP9121 *ctrl, BYTE *c, BYTE *eoi) {
  if (ctrl->hd_hi == ctrl->hd_lo)
    return FALSE;
  if (ctrl->hd_t[ctrl->hd_lo] != 0) {		// wait more
    ctrl->hd_t[ctrl->hd_lo]--;
    return FALSE;
  }
  *c = (BYTE) (ctrl->hd[ctrl->hd_lo] & 0x00FF);
  *eoi = (BYTE) (ctrl->hd[ctrl->hd_lo++] >> 8);
  ctrl->hd_lo &= 0x1FF;
  return TRUE;
}

BOOL hp9121_push_c(VOID *controler, BYTE c) {           	// push on stack ctrl->hc

  HP9121 *ctrl = (HP9121 *) controler;				// cast controler
	
  c &= 0x7F;							// remove parity

  if (c == 0x20 + ctrl->hpibaddr) {				// my listen address
    ctrl->listen = TRUE;
    ctrl->untalk = FALSE;
  } else if (c == 0x40 + ctrl->hpibaddr) {		        // my talk address
    ctrl->talk = TRUE;
    ctrl->untalk = FALSE;
  } else if (c == 0x3F) {					// unlisten
    ctrl->listen = FALSE;
    ctrl->untalk = FALSE;
  } else if (c == 0x5F) {					// untalk
    ctrl->talk = FALSE;
    ctrl->untalk = TRUE;
  } else if ((c & 0x60) == 0x20) {				// listen other, skip
    ctrl->untalk = FALSE;
  } else if ((c & 0x60) == 0x40) {				// talk other, skip
    ctrl->untalk = FALSE;
  } else {							// other
    if ((c == 0x60 + ctrl->hpibaddr) && (ctrl->untalk)) {	// my secondary address after untak -> identify
      ctrl->untalk = FALSE;
      ctrl->st9121 = 2000;
    } else {
      ctrl->untalk = FALSE;
      if (ctrl->talk || ctrl->listen) {				// ok
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: got %02X command\n"), Chipset.Cpu.PC, ctrl->hpibaddr, c );
	  OutputDebugString(buffer);
	}
#endif
	ctrl->hc_t[ctrl->hc_hi] = 5;				// 20 mc68000 cycles of transmission delay
	ctrl->hc[ctrl->hc_hi++] = c;
	ctrl->hc_hi &= 0x1FF;
	_ASSERT(ctrl->hc_hi != ctrl->hc_lo);
      }
    }
  }

  return TRUE;
}

BOOL hp9121_push_d(VOID *controler, BYTE d, BYTE eoi) {	// push on stack ctrl->hd
  HP9121 *ctrl = (HP9121 *) controler;

  if (ctrl->listen || ctrl->talk) {			// only if listen or talk state
    if (((ctrl->hd_hi + 1) & 0x1FF) == ctrl->hd_lo)	// if full, not done
      return FALSE;
    else {
      ctrl->hd_t[ctrl->hd_hi] = 5;			// 20 mc68000 cycles transmission delay
      ctrl->hd[ctrl->hd_hi++] = (WORD) (d | (eoi << 8));
      ctrl->hd_hi &= 0x1FF;
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
//   FSA to simulate the disctrl->c controller (Amigo protocol)
//

static BYTE leds[] = {2, 0, 7};

VOID DoHp9121(HP9121 *ctrl) {
#if defined DEBUG_HP9121
  WORD hp9121st;
#endif
  BYTE eoi;
  // BYTE d;

#if defined DEBUG_HP9121
  hp9121st = ctrl->st9121;
#endif

  if (ctrl->st9121 > 1) {
    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr] + 0 * 2));		// unit 0
    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr] + 1 * 2));		// unit 1
    Chipset.annun |= (1 << (leds[ctrl->hpibaddr] + ctrl->unit * 2));
  }

  switch (ctrl->st9121) {
      // IDLE STATE
    case 0:
      Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr] + 0 * 2));		// unit 0
      Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr] + 1 * 2));		// unit 1
      ctrl->st9121++;
      break;
    case 1:
      if (pop_c(ctrl, &ctrl->c)) {
	ctrl->st9121++;
      }
      break;
    case 2:
      if (ctrl->talk) {
	switch(ctrl->c) {
	  case 0x60:					// secondary 0x00	: Send data
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 20000;
	    break;
	  case 0x68:					// secondary 0x08	: Send Status or address
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 20500;
	    break;
	  case 0x70:					// secondary 0x10   : DSJ
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 21000;
	    break;
	  case 0x71:					// secondary 0x11   : HP-IB CRC
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 21500;
	    break;
	  case 0x7E:					// secondary 0x1E   : Read loopback record
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 22000;
	    break;
	  case 0x7F:					// secondary 0x1F   : Read selftest
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 22500;
	    break;
	  default:
	    _ASSERT(0);
	    ctrl->st9121 = 0;				// restart
	    break;
	  }
      } else if (ctrl->listen) {
	switch(ctrl->c)
	  {
	  case 0x60:					// secondary 0x00	: Receive data
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 10000;
	    break;
	  case 0x68:					// secondary 0x08	: misc
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 1200;
	    break;
	  case 0x69:					// secondary 0x09   : Buffered write ?
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 14000;
	    break;
	  case 0x6A:					// secondary 0x0A   : Buffered read ?
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 14500;
	    break;
	  case 0x6C:					// secondary 0x0C   : misc
	    ctrl->st9121 = 1300;
	    break;
	  case 0x6F:					// secondary 0x0F   : Download
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 17000;
	    break;
	  case 0x70:					// secondary 0x10   : HP-300 clear
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 17500;
	    break;
	  case 0x71:					// secondary 0x11   : HP-IB CRC
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 18000;
	    break;
	  case 0x7E:					// secondary 0x1E   : Write loopback record
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 18500;
	    break;
	  case 0x7F:					// secondary 0x1F   : Initiate selftest
	    ctrl->ppol_e = FALSE;
	    ctrl->st9121 = 19000;
	    break;
	  case 0x04:					// DLC, skip
	    ctrl->st9121 = 0;
	    break;
	  default:
	    _ASSERT(0);
	    ctrl->st9121 = 0;		// restart
	    break;
	  }
      }

      break;
      // receive my secondary address
    case 1000:
      if (ctrl->untalk)
	ctrl->st9121 = 2000;
      else
	ctrl->st9121 = 0;
      ctrl->untalk = FALSE;
      break;
      // Identify command
    case 2000:
      ctrl->d = DELAY_CMD * 4;
      ctrl->st9121++;
      break;
    case 2001:
      if (ctrl->d == 1) ctrl->st9121++;
      ctrl->d--;
      break;
    case 2002:
      if (h_push(ctrl->type[0], 0))
	ctrl->st9121++;		// even if talk not enable ....
      break;
    case 2003:
      if (h_push(ctrl->type[1], 1))
	ctrl->st9121++;
      break;
    case 2004:
#if defined DEBUG_HP9121
      {
	TCHAR buffer[256];
	int k = wsprintf(buffer,_T("%06X: HP9121:%d: Identify = %02X %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->type[0], ctrl->type[1]);
	OutputDebugString(buffer);
      }
#endif
      ctrl->st9121 = 0;
      break;

      // MLA & secondary 08 
    case 1200:
      ctrl->ppol_e = FALSE;
      ctrl->st9121++;
      break;
    case 1201:
      if (pop_d(ctrl, &ctrl->c, &eoi))
	{
	  switch (ctrl->c)
	    {
	    case 0x00:					// op 0x00 : Cold load read
	      ctrl->st9121 = 10500;
	      break;
	    case 0x02:					// op 0x02 : Seek
	      ctrl->st9121 = 11000;
	      break;
	    case 0x03:					// op 0x03 : Request ctrl->Status
	      ctrl->st9121 = 11500;
	      break;
	    case 0x05:					// op 0x05 : Unbuffered read 
	      ctrl->st9121 = 14700;
	      break;
	    case 0x07:					// op 0x07 : Verify
	      ctrl->st9121 = 12000;
	      break;
	    case 0x08:					// op 0x08 : Unbuffered write 
	      ctrl->st9121 = 14100;
	      break;
	    case 0x0B:					// op 0x0B : Initialize
	      ctrl->st9121 = 12500;
	      break;
	    case 0x0C:					// op 0x0C : Set Address Record (for 9133) same as seel without any moving 
	      ctrl->st9121 = 11200;
	      break;
	    case 0x14:					// op 0x14 : Request logical address
	      ctrl->st9121 = 13000;
	      break;
	    case 0x15:					// op 0x15 : End
	      ctrl->st9121 = 13500;
	      break;
	    default:
	      _ASSERT(0);
	      ctrl->ppol_e = FALSE;
	      ctrl->st9121 = 0;				// restart
	      break;
	    }
	}
      break;
      // MLA & secondary 0C 
    case 1300:
      ctrl->ppol_e = FALSE;
      ctrl->st9121++;
      break;
    case 1301:
      if (pop_d(ctrl, &ctrl->c, &eoi))
	{
	  switch (ctrl->c)
	    {
	    case 0x05:					// op 0x05 : Unbuffered read verify
	      ctrl->st9121 = 15000;
	      break;
	    case 0x14:					// op 0x14 : Request physical address
	      ctrl->st9121 = 15500;
	      break;
	    case 0x16:					// op 0x16 : Send wear
	      ctrl->st9121 = 16000;
	      break;
	    case 0x18:					// op 0x18 : Format
	      ctrl->st9121 = 16500;
	      break;
	    case 0x19:					// op 0x19 : Door lock (9133)
	      ctrl->st9121 = 9000;
	      break;
	    case 0x1A:					// op 0x1A : Door unlock (9133)
	      ctrl->st9121 = 9000;
	      break;
	    default:
	      _ASSERT(0);
	      ctrl->st9121 = 0;				// restart
	      break;
	    }
	}
      break;

      // get a byte and leave
    case 9000:
      if (ctrl->listen) {
	if (pop_d(ctrl, &ctrl->c, &eoi)) {
	  ctrl->ppol_e = TRUE;
	  ctrl->st9121 = 0;				// done
	}
      }

      // MLA & secondary 00 : receive data
    case 10000:
      if (ctrl->listen)
	{
	  if (pop_d(ctrl, &ctrl->c, &eoi))
	    {
	      ctrl->disk[ctrl->unit][ctrl->addr[ctrl->unit]++] = ctrl->c;
	      if ((ctrl->addr[ctrl->unit] & 0xFF) == 0x00)					// one sector done
		{
		  if (ctrl->addr[ctrl->unit] == 0x00000100) {
		    GetLifName(ctrl, ctrl->unit);
		    kmlButtonText6(1 + ctrl->hpibaddr + ctrl->unit, ctrl->lifname[ctrl->unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3
		  }
		  if (ctrl->unbuffered) {
		    inc_addr(ctrl, ctrl->unit);
		  } else {
		    inc_addr(ctrl, ctrl->unit);
		    ctrl->ppol_e = TRUE;
		    ctrl->st9121 = 0;
		  }
		}
	      if (eoi) {
		ctrl->ppol_e = TRUE;
		ctrl->st9121 = 0;
	      }
	    }
	} else {
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;				// unlisten, stop !!!
      }
      break;
      // MLA & secondary 08 & 0x00 : cold load read
    case 10500:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {		// got hhssssss
	ctrl->d = ctrl->c;
	ctrl->st9121++;
      }
      break;
    case 10501:
      ctrl->unit = 0;
      ctrl->cylinder[ctrl->unit] = 0;
      ctrl->head[ctrl->unit] = (ctrl->c >> 6);
      ctrl->sector[ctrl->unit] = (ctrl->c & 0x3F);
      raj_addr(ctrl, ctrl->unit);
      ctrl->s1[0] = 0x1F;					// drive attention (seek)
      ctrl->s1[1] = ctrl->unit;
      ctrl->s2[0] = 0x0C;					// formatted disc
      ctrl->s2[1] = 0x80;					// drive attention (seek completed)
      ctrl->dsj = 0;
      ctrl->ppol_e = TRUE;
      ctrl->st9121 = 0;				

      // MLA & secondary 08 & 0x02 : seek
    case 11000:
      if (pop_d(ctrl, &ctrl->c, &eoi))
	{
	  ctrl->d = ctrl->c;
	  ctrl->st9121++;
	}
    case 11001:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {	
	if (ctrl->d < 2)
	  ctrl->cylinder[ctrl->d] = ctrl->c << 8;
	ctrl->st9121++;
      }
      break;
    case 11002:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->d < 2)
	  ctrl->cylinder[ctrl->d] |= ctrl->c;
	ctrl->st9121++;
      }
      break;
    case 11003:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->d < 2)
	  ctrl->head[ctrl->d] = ctrl->c;
	ctrl->st9121++;
      }
      break;
    case 11004:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->d < 2) {
	  ctrl->unit = (BYTE) ctrl->d;			// good unit
	  ctrl->sector[ctrl->unit] = ctrl->c;
	  if (ctrl->disk[ctrl->unit] != NULL)	{	//  disk
	    raj_addr(ctrl, ctrl->unit);
	    ctrl->s1[0] = 0x1F;				// drive attention (seek)
	    ctrl->s1[1] = ctrl->unit;
	    ctrl->s2[0] = 0x0C;				// formatted disc
	    ctrl->s2[1] = 0x80;				// drive attention (seek completed)
	    ctrl->dsj = 0;
	  } else {					// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x1F;				// drive attention (seek)
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x2D;				// *TTTTR     00101101
	    ctrl->s2[1] = 0x84;				// AW/EFCSS   10000100
	  }
	} else {					// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;				// 00dSSSSS 	unit unavailable
	  ctrl->s1[1] = (BYTE) ctrl->d;			// unit
	  ctrl->s2[0] = 0x00;				// *TTTTR    
	  ctrl->s2[1] = 0x00;				// AW/EFCSS 	no drive connected
	}
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: Seek unit %d -> %08X (s:%02X,h:%d,c:%04X)\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, ctrl->addr[ctrl->unit], ctrl->sector[ctrl->unit], ctrl->head[ctrl->unit], ctrl->cylinder[ctrl->unit]);
	  OutputDebugString(buffer);
	}
#endif
      }
      break;
      // MLA & secondary 08 & 0x0C : Set Address record (9133)
    case 11200:
      ctrl->st9121 = 11000;
      break;

      // MLA & secondary 08 & 0x03 : request Status
    case 11500:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	ctrl->message[0] = ctrl->s1[0];
	ctrl->message[1] = ctrl->s1[1];
	ctrl->message[2] = ctrl->s2[0];
	ctrl->message[3] = ctrl->s2[1];
	ctrl->s2[1] &= 0x63;				// clear bits
	ctrl->dsj = 0;					// DSJ = 0
	if (ctrl->c > 1) {				// bad unit (9133)
	  ctrl->message[0] = 0x17;
	  ctrl->message[1] = ctrl->c;
	  ctrl->message[2] = 0;
	  ctrl->message[3] = 0;
	}
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;
      }
      break;
      // MLA & secondary 08 & 0x07 : verify (stop on last good cylinder)
    case 12000:						// fake it
      if (pop_d(ctrl, &ctrl->c, &eoi)) {		// unit
	ctrl->d = ctrl->c;
	ctrl->st9121++;
      }
      break;
    case 12001:					
      if (pop_d(ctrl, &ctrl->c, &eoi)) {		// sector high
	if (ctrl->d < 2) 
	  raj_addr(ctrl, (BYTE) ctrl->d);
	ctrl->st9121++;
      }
      break;
    case 12002:					
      if (pop_d(ctrl, &ctrl->c, &eoi)) {		// sector low
	if (ctrl->d < 2) {
	  ctrl->unit = (BYTE) ctrl->d;
	  ctrl->sector[ctrl->unit] = ctrl->c;
	  raj_addr(ctrl, ctrl->unit);
	  if (ctrl->disk[ctrl->unit] != NULL) {	//  disk
	    ctrl->sector[ctrl->unit] = 0;
	    ctrl->head[ctrl->unit] = 0;
	    ctrl->cylinder[ctrl->unit] = ctrl->config_cylinders-1;		// last cylinder
	    raj_addr(ctrl, ctrl->unit);
	    ctrl->s1[0] = 0;
	    ctrl->s1[1] = 0;
	    ctrl->dsj = 0;
	  } else {					// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x13;				// 00dSSSSS : 00010011
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x0D;				// *TTTTR     00101101
	    ctrl->s2[1] = 0x83;				// AW/EFCSS   10000011
	  }
	} else {									// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;					// 00dSSSSS : 00010111		unit unavailable
	  ctrl->s1[1] = (BYTE) ctrl->d;				// unit
	  ctrl->s2[0] = 0x00;					// *TTTTR     
	  ctrl->s2[1] = 0x00;					// AW/EFCSS  
	}
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;
      }
      break;
      // MLA & secondary 08 & 0x0B : initialize
    case 12500:
      _ASSERT(0);
      break;
      // MLA & secondary 08 & 0x14 : request logical address
    case 13000:							// dummy byte
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	ctrl->st9121++;
      }
      break;
    case 13001:
      ctrl->message[0] = ctrl->cylinder[ctrl->unit] >> 8;
      ctrl->message[1] = ctrl->cylinder[ctrl->unit] & 0xFF;
      ctrl->message[2] = ctrl->head[ctrl->unit];
      ctrl->message[3] = ctrl->sector[ctrl->unit];
      ctrl->ppol_e = TRUE;
      ctrl->st9121 = 0;
      break;
      // MLA & secondary 08 & 0x15 : end
    case 13500:
      _ASSERT(0);
      break;
      // MLA & secondary 09 : buffered write ?
    case 14000:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->c == 0x08)
	  ctrl->st9121++;
	else {
	  ctrl->ppol_e = TRUE;
	  ctrl->st9121 = 0;
	}
      }
      break;
    case 14001:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->c < 2) {
	  ctrl->unit = ctrl->c;
	  raj_addr(ctrl, ctrl->unit);
	  if (ctrl->disk[ctrl->unit] != NULL)	{	//  disk
	    ctrl->s1[0] = 0;
	    ctrl->s1[1] = ctrl->unit;
	    ctrl->dsj = 0;
	  } else {								// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x13;					// 00dSSSSS : 00010011
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x0D;					// *TTTTR     00001101
	    ctrl->s2[1] = 0x03;					// AW/EFCSS   00000011
	  }
	} else {									// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;					// 00dSSSSS 		unit unavailable
	  ctrl->s1[1] = ctrl->c;				// unit
	  ctrl->s2[0] = 0x00;					// *TTTTR    
	  ctrl->s2[1] = 0x00;					// AW/EFCSS   
	}
	ctrl->ppol_e = TRUE;
	ctrl->unbuffered = 0;
	ctrl->st9121 = 0;
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: buffered write unit %d -> %08X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, ctrl->addr[ctrl->unit]);
	  OutputDebugString(buffer);
	}
#endif
      }
      break;
      // MLA & secondary 08 op 8 : unbuffered write
    case 14100:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->c < 2) {
	  ctrl->unit = ctrl->c;
	  raj_addr(ctrl, ctrl->unit);
	  if (ctrl->disk[ctrl->unit] != NULL)	{	//  disk
	    ctrl->s1[0] = 0;
	    ctrl->s1[1] = 0;
	    ctrl->dsj = 0;
	  } else {								// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x13;					// 00dSSSSS : 00010011
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x0D;					// *TTTTR     00001101
	    ctrl->s2[1] = 0x03;					// AW/EFCSS   00000011
	  }
	} else {									// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;					// 00dSSSSS : 00010111		unit unavailable
	  ctrl->s1[1] = ctrl->c;				// unit
	  ctrl->s2[0] = 0x2D;					// *TTTTR     00100001
	  ctrl->s2[1] = 0x82;					// AW/EFCSS   10000010		no drive connected
	}
	ctrl->ppol_e = TRUE;
	ctrl->unbuffered = 1;
	ctrl->st9121 = 0;
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: unbuffered write unit %d -> %08X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, ctrl->addr[ctrl->unit]);
	  OutputDebugString(buffer);
	}
#endif
      }
      break;
			
      // MLA & secondary 0A op 05 : buffered read 
    case 14500:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->c == 0x05)
	  ctrl->st9121++;
	else {
#if defined DEBUG_HP9121
	  {
	    TCHAR buffer[256];
	    int k = wsprintf(buffer,_T("%06X: HP9121:%d: MLA SEC0A op %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->c);
	    OutputDebugString(buffer);
	  }
#endif
	  _ASSERT(0);
	  ctrl->ppol_e = TRUE;
	  ctrl->st9121 = 0;
	}
      }
      break;
    case 14501:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->c < 2) {
	  ctrl->unit = ctrl->c;
	  if (ctrl->disk[ctrl->unit] != NULL)	{	//  disk
	    ctrl->s1[0] = 0;
	    ctrl->s1[1] = 0;
	    ctrl->dsj = 0;
	  } else {					// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x13;				// 00dSSSSS : 00010011
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x0D;				// *TTTTR     00001101
	    ctrl->s2[1] = 0x03;				// AW/EFCSS   00000011
	  }
	} else {					// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;				// 00dSSSSS 	unit unavailable
	  ctrl->s1[1] = ctrl->c;			// unit
	  ctrl->s2[0] = 0x00;				// *TTTTR     
	  ctrl->s2[1] = 0x00;				// AW/EFCSS 	no drive connected
	}
	ctrl->ppol_e = TRUE;
	ctrl->unbuffered = 0;
	ctrl->st9121 = 0;
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: buffered read unit %d -> %08X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, ctrl->addr[ctrl->unit]);
	  OutputDebugString(buffer);
	}
#endif
      }
      break;
      // MLA & secondary 08 op 05 : unbuffered read 
    case 14700:
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->c < 2) {
	  ctrl->unit = ctrl->c;
	  if (ctrl->disk[ctrl->unit] != NULL)	{	//  disk
	    ctrl->s1[0] = 0;
	    ctrl->s1[1] = 0;
	    ctrl->dsj = 0;
	  } else {					// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x13;				// 00dSSSSS : 00010011
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x0D;				// *TTTTR     00001101
	    ctrl->s2[1] = 0x03;				// AW/EFCSS   00000011
	  }
	} else {					// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;				// 00dSSSSS :		unit unavailable
	  ctrl->s1[1] = ctrl->c;			// unit
	  ctrl->s2[0] = 0x00;				// *TTTTR    
	  ctrl->s2[1] = 0x00;				// AW/EFCSS  	no drive connected
	}
	ctrl->ppol_e = TRUE;
	ctrl->unbuffered = 1;
	ctrl->st9121 = 0;
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: unbuffered read unit %d -> %08X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, ctrl->addr[ctrl->unit]);
	  OutputDebugString(buffer);
	}
#endif
      }
      break;
      // MLA & secondary 0C & 0x05 : unbuffered read verify
    case 15000:
      _ASSERT(0);
      break;
      // MLA & secondary 0C & 0x14 : request physical address
    case 15500:
      _ASSERT(0);
      ctrl->message[0] = ctrl->cylinder[ctrl->unit] >> 8;
      ctrl->message[1] = ctrl->cylinder[ctrl->unit] & 0xFF;
      ctrl->message[2] = ctrl->head[ctrl->unit];
      ctrl->message[3] = ctrl->sector[ctrl->unit];
      ctrl->ppol_e = TRUE;
      ctrl->st9121 = 0;
      break;
      // MLA & secondary 0C & 0x16 : send wear
    case 16000:
      _ASSERT(0);
      ctrl->ppol_e = TRUE;
      ctrl->st9121 = 0;
      break;
      // MLA & secondary 0C & 0x18 : format
    case 16500:							// unit
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	ctrl->d = ctrl->c;
	ctrl->st9121++;
      }
      break;
    case 16501:							// type
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	ctrl->st9121++;
      }
      break;
    case 16502:							// interleave
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	ctrl->st9121++;
      }
      break;
    case 16503:							// data
      if (pop_d(ctrl, &ctrl->c, &eoi)) {
	if (ctrl->d < 2) {
	  ctrl->unit = (BYTE) ctrl->d;
	  if (ctrl->disk[ctrl->unit] != NULL)	{	//  disk
	    memset(ctrl->disk[ctrl->unit], ctrl->c, DISK_BYTES);
	    ctrl->s1[0] = 0;
	    ctrl->s1[1] = 0;
	    ctrl->dsj = 0;
	    ctrl->sector[ctrl->unit] = 0;
	    ctrl->head[ctrl->unit] = 0;
	    ctrl->cylinder[ctrl->unit] = 0;
	  } else {					// no disk
	    ctrl->dsj = 1;
	    ctrl->s1[0] = 0x13;				// 00dSSSSS : 00010011
	    ctrl->s1[1] = ctrl->unit;			// unit
	    ctrl->s2[0] = 0x0D;				// *TTTTR     00001101
	    ctrl->s2[1] = 0x03;				// AW/EFCSS   00000011
	  }
	} else {					// no unit
	  ctrl->dsj = 1;
	  ctrl->s1[0] = 0x17;				// 00dSSSSS : 00010111		unit unavailable
	  ctrl->s1[1] = (BYTE) ctrl->d;			// unit
	  ctrl->s2[0] = 0x2D;				// *TTTTR     00100001
	  ctrl->s2[1] = 0x82;				// AW/EFCSS   10000010		no drive connected
	}
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;
      }
      break;
      // MLA & secondary 0F : download
    case 17000:
      _ASSERT(0);
      break;
      // MLA & secondary 10 : HP-300 clear
    case 17500:
      ctrl->ppol_e = FALSE;
      if (pop_d(ctrl, &ctrl->c, &eoi)) {	// get dummy byte
	ctrl->st9121++;
      }
      break;
    case 17501:
      if (pop_c(ctrl, &ctrl->c)) {	// get Selected Device Clear command
	ctrl->st9121++;
      }
      break;
    case 17502:
      ctrl->s1[0] = 0;
      ctrl->s1[1] = 0;
      ctrl->s2[0] = 0;
      ctrl->s2[1] = 0;
      ctrl->dsj = 0;
      ctrl->unit = 0;
      ctrl->ppol_e = TRUE;
      ctrl->st9121 = 0;
      break;
      // MLA & secondary 11 : HP-IB CRC
    case 18000:
      _ASSERT(0);
      break;
      // MLA & secondary 1E : write loopback record
    case 18500:
      _ASSERT(0);
      break;
      // MLA & secondary 1F : initiate selftest
    case 19000:
      _ASSERT(0);
      break;

      // MTA & secondary 00 : send data
    case 20000:
      ctrl->d = DELAY_CMD * 4;			// wait a bit
      ctrl->st9121++;
      break;
    case 20001:
      if (ctrl->d == 1) ctrl->st9121++;
      ctrl->d--;
      break;
    case 20002:
      ctrl->d = 256;
      ctrl->st9121++;
      break;
    case 20003:
      if (ctrl->talk) {
	if (ctrl->d > 0) {
	  if (ctrl->dsj == 0) {
	    if (h_push(ctrl->disk[ctrl->unit][ctrl->addr[ctrl->unit]], (ctrl->d == 1) ? !ctrl->unbuffered : 0)) {
	      ctrl->addr[ctrl->unit]++;
	      ctrl->d--;
	    }
	  } else {
	    if (h_push(0x00, 0x80))		// error DSJ != 0
	      ctrl->d--;
	  }
	}
	if (ctrl->d == 0) {
	  if (ctrl->dsj == 0) inc_addr(ctrl, ctrl->unit);
	  if (ctrl->unbuffered) {
#if defined DEBUG_HP9121
	    {
	      TCHAR buffer[256];
	      int k = wsprintf(buffer,_T("%06X: HP9121:%d: 256 bytes done ...\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	      OutputDebugString(buffer);
	    }
#endif
	    ctrl->d = 256;
	  } else {
#if defined DEBUG_HP9121
	      {
		TCHAR buffer[256];
		int k = wsprintf(buffer,_T("%06X: HP9121:%d: 256 bytes finished\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
		OutputDebugString(buffer);
	      }
#endif
	      ctrl->ppol_e = TRUE;
	      ctrl->st9121 = 0;
	  }
	}
      } else {
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: stopped by untalk, done %d ...\n"), Chipset.Cpu.PC, ctrl->hpibaddr, 256 - ctrl->d);
	  OutputDebugString(buffer);
	}
#endif
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;				// untalk ... stop
      }
      break;
    case 20004:						// send a 1 with eoi for 9133 
      if (h_push(0x01, 0x80)) {
#if defined DEBUG_HP9121
	{
	  TCHAR buffer[256];
	  int k = wsprintf(buffer,_T("%06X: HP9121:%d: 9133 finished\n"), Chipset.Cpu.PC, ctrl->hpibaddr);
	  OutputDebugString(buffer);
	}
#endif
	ctrl->ppol_e = TRUE;
	ctrl->st9121 = 0;
      }
      break;

      // MTA & secondary 08 : send status or address
    case 20500:
      ctrl->d = DELAY_CMD * 4;			// wait a bit
      ctrl->st9121++;
      break;
    case 20501:
      if (ctrl->d == 1) ctrl->st9121++;
      ctrl->d--;
      break;
    case 20502:
      if (ctrl->talk) {
	if (h_push(ctrl->message[0], 0x00))
	  ctrl->st9121++;
      }
      break;
    case 20503:
      if (ctrl->talk) {
	if (h_push(ctrl->message[1], 0x00))
	  ctrl->st9121++;
      }
      break;
    case 20504:
      if (ctrl->talk) {
	if (h_push(ctrl->message[2], 0x00))
	  ctrl->st9121++;
      }
      break;
    case 20505:
      if (ctrl->talk) {
	if (h_push(ctrl->message[3], 0x80)) {
	  ctrl->st9121++;
	}
      }
      break;
    case 20506:
      ctrl->ppol_e = TRUE;
      ctrl->st9121 = 0;
      break;
      // MTA & secondary 10 : DSJ
    case 21000:
#if defined DEBUG_HP9121
      {
	TCHAR buffer[256];
	int k = wsprintf(buffer,_T("%06X: HP9121:%d: DSJ = %02X\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->dsj);
	OutputDebugString(buffer);
      }
#endif
      ctrl->d = DELAY_CMD * 4;			// wait a bit
      ctrl->st9121++;
      break;
    case 21001:
      if (ctrl->d == 1) ctrl->st9121++;
      ctrl->d--;
      break;
    case 21002:
      if (ctrl->talk) {
	if (h_push(ctrl->dsj, 0x80))		// send DSJ = 0 and EOI
	  ctrl->st9121++;
      }
      break;
    case 21003:
      if (ctrl->dsj == 2)
	ctrl->dsj = 0;
      ctrl->st9121 = 0;
      break;

      // MTA & secondary 11 : HP-IB CRC
    case 21500:
      _ASSERT(0);
      break;
      // MTA & secondary 1E : read loopback record
    case 22000:
      _ASSERT(0);
      break;
      // MTA & secondary 1E : read selftest
    case 22500:
      _ASSERT(0);
      break;
    default:
      _ASSERT(0);
      break;
    }


  // check status
	

#if defined DEBUG_HP9121
  {
    TCHAR buffer[256];
    TCHAR *TXT = HP9121_LAB((WORD)(ctrl->st9121));
    int k;
    //		unsigned long l;

    if ((hp9121st != ctrl->st9121) && ((ctrl->st9121 > 2) || (hp9121st > 2)))
      {
	if (TXT == NULL)
	  k = wsprintf(buffer,_T("%06X: HP9121:%d:%d: %u->%u\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, hp9121st, ctrl->st9121);
	else
	  k = wsprintf(buffer,_T("%06X: HP9121:%d:%d: %u->%u ( %s )\n"), Chipset.Cpu.PC, ctrl->hpibaddr, ctrl->unit, hp9121st, ctrl->st9121, TXT);
#if defined DEBUG_ONDISK
	WriteFile(hDebug, buffer, (unsigned long) (k), &l, NULL); 
	//			FlushFileBuffers(hDebug);
#endif
	OutputDebugString(buffer);
      }
  }
#endif
}

BOOL hp9121_load(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename) {
  INT   hDiskFile = -1;
  DWORD dwFileSize, dwRead;
  LPBYTE pDisk = NULL;
  struct stat rs;

  hDiskFile = open(szFilename, O_RDWR);
	
  if (hDiskFile < 0) {
    hDiskFile = -1;
    return FALSE;
  }
  fstat(hDiskFile,&rs);
  dwFileSize = rs.st_size;
  if (dwFileSize > (DWORD)(DISK_BYTES)){ // file is too big.
    close(hDiskFile);
    return FALSE;
  }

  pDisk = malloc(DISK_BYTES);
  if (pDisk == NULL) {
    close(hDiskFile);
    return FALSE;
  }
  bzero(pDisk,DISK_BYTES);
  // load file content)
  dwRead = read(hDiskFile, pDisk, dwFileSize);
  close(hDiskFile);
  _ASSERT(dwFileSize == dwRead);

  if (ctrl->disk[unit] != NULL)
    free(ctrl->disk[unit]);
  ctrl->disk[unit] = pDisk;
  //	ctrl->Status[unit] = 0x0001;			// disk present

  strcpy(ctrl->name[unit], szFilename);

  Chipset.annun |= (1 << (leds[ctrl->hpibaddr] + 1 + unit * 2));
  UpdateAnnunciators(FALSE);

  GetLifName(ctrl, unit);
  kmlButtonText6(1 + ctrl->hpibaddr + unit, ctrl->lifname[unit], -12, 19); // 6 bytes of text -16,16 pixels down of buttons 2 or 3

  return TRUE;
}

BOOL hp9121_save(HP9121 *ctrl, BYTE unit, LPCTSTR szFilename) {
  INT   hDiskFile = -1;
  DWORD dwWritten;

  if (ctrl->disk[unit] == NULL)
    return FALSE;

  hDiskFile = creat(szFilename,O_WRONLY);
  if (hDiskFile < 0)    {
    hDiskFile = -1;
    return FALSE;
  }

  // save file content
  dwWritten = write(hDiskFile, ctrl->disk[unit], DISK_BYTES);
  close(hDiskFile);
  strcpy(ctrl->name[unit], szFilename);

  _ASSERT((DWORD)(DISK_BYTES) == dwWritten);

  return TRUE;
}

BOOL hp9121_eject(HP9121 *ctrl, BYTE unit) {
  if (ctrl->disk[unit] != NULL)  free(ctrl->disk[unit]);
  ctrl->name[unit][0] = 0x00;
  ctrl->disk[unit] = NULL;

  Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr]+ 1 + unit * 2));
  UpdateAnnunciators(FALSE);

  ctrl->lifname[unit][0] = 0x00;
  kmlButtonText6(1 + ctrl->hpibaddr + unit, ctrl->lifname[unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3

  return TRUE;
}

BOOL hp9121_widle(HP9121 *ctrl) {
  DWORD dwRefTime;

  dwRefTime = timeGetTime();
  //Chipset.idle = FALSE;
  usleep(10000);
  // wait for the Iddle state with 5 sectrl->c timeout
  while (((timeGetTime() - dwRefTime) < 5000L) && !(ctrl->st9121 < 2))
    usleep(10000);

  if (ctrl->st9121 < 2)				// not timeout, hp9121 is idle
    return FALSE;				// idle state

  return TRUE;					// hp9121 was busy
}

VOID hp9121_reset(VOID *controler) {
  HP9121 *ctrl = (HP9121 *) controler;
  BYTE i;

  ctrl->type[0] = 0x01;			// $0104   82901
  ctrl->type[1] = 0x04;
  ctrl->config_heads = 2;
  ctrl->config_cylinders = 35;		// 35 - 2 spares ...
  ctrl->config_sectors = 16;
  
  for (i = 0; i < 1; i++) {
    if (ctrl->name[i][0] != 0x00) {
      hp9121_load(ctrl, i, ctrl->name[i]);
    }
    if (ctrl->name[i][0] != 0x00) {
      Chipset.annun |= (1 << (leds[ctrl->hpibaddr] + 1 + ctrl->unit * 2));
    } else {
      Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr] + 1 + ctrl->unit * 2));
    }
    ctrl->head[i] = 0;
    ctrl->cylinder[i] = 0;
    ctrl->sector[i] = 0;
    ctrl->addr[i] = 0;
  }
  ctrl->unit = 0;
  ctrl->st9121 = 0;				// state of hp9121 controller
  ctrl->untalk = FALSE;				// previous command was UNTALK ?
  ctrl->talk = FALSE;				// MTA received ?
  ctrl->listen = FALSE;				// MLA received ?
  ctrl->ppol_e = TRUE;				// parallel poll enabled
  ctrl->hc_hi = 0;				// hi mark
  ctrl->hc_lo = 0;				// lo mark
  ctrl->hd_hi = 0;				// hi mark
  ctrl->hd_lo = 0;				// lo mark
  ctrl->dsj = 0;
  ctrl->s1[0] = 0;
  ctrl->s1[1] = 0;
  ctrl->s2[0] = 0;
  ctrl->s2[1] = 0;
}

VOID hp9121_stop(VOID *controler) {
  HP9121 *ctrl = (HP9121 *) controler;
  int unit;

  for (unit = 0; unit < 1; unit++) {
    if (ctrl->disk[unit] != NULL)
      free(ctrl->disk[unit]);
    ctrl->disk[unit] = NULL;

    Chipset.annun &= ~(1 << (leds[ctrl->hpibaddr]+ 1 + unit * 2));
    UpdateAnnunciators(FALSE);

    ctrl->lifname[unit][0] = 0x00;
    kmlButtonText6(1 + ctrl->hpibaddr + unit, ctrl->lifname[unit], -12, 19);		// 6 bytes of text -16,16 pixels down of buttons 2 or 3
  }
}
