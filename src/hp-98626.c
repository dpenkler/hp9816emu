/*
 *   Hp-98626.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   serial 98626 for 9816 at sc 9
//	 non functional, just allow 9816 boot without error
//	 taken from atarist emulator

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"								// I/O definitions

#define DEBUG_98626						// debug flag

#if defined DEBUG_98626
	TCHAR buffer[256];
	int k;
#endif

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
// write in ACIAs IO space
// 

static DWORD ticks = 0;

BYTE Write_98626(BYTE *a, WORD d, BYTE s) {
  if (s != 1) {
    fprintf(stderr,"98626 write acces not in byte !!");
    return BUS_ERROR;
  }
  if (s == 1) {
    switch (d) {
    case 0x0011:	
      Chipset.Serial.regs[0] = *(--a);
      return BUS_OK;
      break;
    case 0x0013:
      Chipset.Serial.regs[1] = *(--a);
      return BUS_OK;
      break;
    case 0x0015:
      Chipset.Serial.regs[2] = *(--a);
      return BUS_OK;
      break;
    case 0x0017:
      Chipset.Serial.regs[3] = *(--a);
      return BUS_OK;
      break;
    case 0x0019:
      Chipset.Serial.regs[4] = *(--a);
      return BUS_OK;
      break;
    case 0x001B:
      Chipset.Serial.regs[5] = *(--a);
      return BUS_OK;
      break;
    case 0x001D:
      Chipset.Serial.regs[6] = *(--a);
      return BUS_OK;
      break;
    case 0x001F:
      Chipset.Serial.regs[7] = *(--a);
      return BUS_OK;
      break;
    }
    return BUS_OK;
  } else return BUS_ERROR;
}

//
// read in ACIAs IO space
//
BYTE Read_98626(BYTE *a, WORD d, BYTE s)
{
  if (s != 1) {
    fprintf(stderr,"98626 read acces not in byte !!");
    return BUS_ERROR;
  }
  if (s == 1) {
    switch (d) {
    case 0x0001:		// status reg 0 : 2 : serial, 130 : remote jumper removed
      *(--a) = 0x02;
      return BUS_OK;
      break;
    case 0x0003:		// status reg 1
      *(--a) = 0x10;
      return BUS_OK;
      break;
    case 0x0005:		// status reg 3
      *(--a) = 0x00;
      return BUS_OK;
      break;
    case 0x0007:		// status reg 5
      *(--a) = 0xC0;
      return BUS_OK;
      break;

    case 0x0011:		// 8250 reg 0
      *(--a) = Chipset.Serial.regs[0];
      return BUS_OK;
      break;
    case 0x0013:		// 8250 reg 1
      *(--a) = Chipset.Serial.regs[1];
      return BUS_OK;
      break;
    case 0x0015:
      *(--a) = Chipset.Serial.regs[2];
      return BUS_OK;
      break;
    case 0x0017:
      *(--a) = Chipset.Serial.regs[3];
      return BUS_OK;
      break;
    case 0x0019:
      *(--a) = Chipset.Serial.regs[4];
      return BUS_OK;
      break;
    case 0x001B:
      *(--a) = Chipset.Serial.regs[5];
      return BUS_OK;
      break;
    case 0x001D:
      *(--a) = Chipset.Serial.regs[6];
      return BUS_OK;
      break;
    case 0x001F:
      *(--a) = Chipset.Serial.regs[7];
      return BUS_OK;
      break;

    default:
      return BUS_OK;
    }
    return BUS_OK;
  } else return BUS_ERROR;
}

VOID Send_To_Mc6850K(BYTE c) {							// send a byte to acia keyboard in fifo
  Chipset.Serial.fifo_in[Chipset.Serial.fifo_in_t++] = c;			// push data
  Chipset.Serial.fifo_in_t &= 0x3F;						// wrap
#if defined DEBUG_98626
  k = sprintf(buffer,"%06x: MC6850 : GOT %02x\n", (UINT)Chipset.Cpu.PC, (UINT)c);
  // OutputDebugString(buffer); buffer[0] = 0x00;
#endif
}

static BYTE get_data() {						// get a byte from fifo
  BYTE data;

  if (Chipset.Serial.fifo_in_b != Chipset.Serial.fifo_in_t)	{	// some data
    data = Chipset.Serial.fifo_in[Chipset.Serial.fifo_in_b++];		// pop data
    Chipset.Serial.fifo_in_b &= 0x3F;					// wrap
  } else data = 0xFF;							// no data 
  return data;
}

VOID Do_Acia_Keyboard() {
  //	BYTE data;

  ticks += Chipset.dcycles;
  if (ticks < 6000) return;
  ticks -= 6000;

  if ((Chipset.Serial.status & 0x01) == 0x00) {				// ready to send a new byte
    if (Chipset.Serial.fifo_in_t != Chipset.Serial.fifo_in_b) {		// some key to send back to ST via 68901 interrupt
      Chipset.Serial.data_in = get_data();
      Chipset.Serial.status |= 0x01;									// set Receive Data Register Full
      if (Chipset.Serial.control & 0x80) {								// CR7 enable interrupt
#if defined DEBUG_98626
	k = sprintf(buffer,"        : MC6850 : INTERRUPT !\n");
	//	OutputDebugString(buffer); buffer[0] = 0x00;
#endif
	Chipset.Serial.status |= 0x80;									// set interrupt wanted
	Chipset.Serial.inte = 1;											// want an in from MFP901
	// In_Mfp901(MFP_KEYB_INT_LINE, 1);								// set line /IRQ
      }
    }
  }
  if ((Chipset.Serial.status & 0x02) == 0x00) {				// not Transmit Data Register Empty
    //if (Chipset.keyboard.status) Chipset.acia1.status |= 0x02;								// empty it
  }
}

VOID Reset_Mc6850(VOID) {
  ticks = 0;
  bzero((BYTE *) &Chipset.Serial, sizeof(HP98626));
}
