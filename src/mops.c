/*
 *   Mops.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   Low level chipset, bus and CPU stuff based on docs
//
//

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"



//################
//#
//#    Public functions
//#
//################

//
// reset whole system
//
VOID systemReset(VOID) {		// register settings after System Reset

  Chipset.Cpu.A[0].l = 0x00000000;
  Chipset.Cpu.A[1].l = 0x00000000;
  Chipset.Cpu.A[2].l = 0x00000000;
  Chipset.Cpu.A[3].l = 0x00000000;
  Chipset.Cpu.A[4].l = 0x00000000;
  Chipset.Cpu.A[5].l = 0x00000000;
  Chipset.Cpu.A[6].l = 0x00000000;
  Chipset.Cpu.A[7].l = 0x00000000;
  Chipset.Cpu.A[8].l = 0x00000000;
  Chipset.Cpu.D[0].l = 0x00000000;
  Chipset.Cpu.D[1].l = 0x00000000;
  Chipset.Cpu.D[2].l = 0x00000000;
  Chipset.Cpu.D[3].l = 0x00000000;
  Chipset.Cpu.D[4].l = 0x00000000;
  Chipset.Cpu.D[5].l = 0x00000000;
  Chipset.Cpu.D[6].l = 0x00000000;
  Chipset.Cpu.D[7].l = 0x00000000;

  Chipset.Cpu.SR.sr = 0x2700;

  Chipset.Cpu.State = EXCEPTION;
  Chipset.Cpu.lastVector = 0;

  Chipset.I210 = 0;
  Chipset.cycles = 0x00000000;
  Chipset.dcycles = 0x0000;
  Chipset.ccycles = 0x00000000;

  bzero(Chipset.Ram, Chipset.RamSize);
	
  Chipset.Keyboard.ram[0x12] = 0x00;	// us keyboard
  Chipset.annun &= 0x003FFFF;

  // load initial pc and ssp
  ReadMEM((BYTE *)& Chipset.Cpu.PC, 0x000004, 4);
  ReadMEM((BYTE *)& Chipset.Cpu.A[8], 0x000000, 4);

  reloadGraph();
  updateAlpha(TRUE);			// refresh whole screen

  hpib_stop_bus();			// stop hpib bus activity
  hpib_init_bus();			// initialize hpib bus
  Reset_Keyboard();			// reset the keyboard 8041 

  // end of test

  hpib_names();				// display lif names
  updateLeds(TRUE);			// update all LEDS
}

////////////////////////////////////////////////////////////////////////////////
//
//   Bus Commands: for mc68000
//		be careful of endianness
//
////////////////////////////////////////////////////////////////////////////////

// 
// read a word from RAM and ROM only for debug and disasm
//
WORD GetWORD(DWORD d) {
  DWORD ad = d & 0x00FFFFFF;
  WORD data = 0xFFFF;
  BYTE *a = (BYTE *) &data;

  a += 2;
  if (ad < (Chipset.RomSize - 1))				// ROM
    {
      *(--a) = Chipset.Rom[ad++];
      *(--a) = Chipset.Rom[ad++];
    }
  else if ((ad >= Chipset.RamStart) && (ad < 0x00FFFFFF))	// RAM
    {
      ad -= Chipset.RamStart;

      *(--a) = Chipset.Ram[ad++];
      *(--a) = Chipset.Ram[ad++];
    }
  return data;
}
//
// read at address d to *a with s byte size
//   return BUS_OK, BUS_ERROR or ADRESS_ERROR
//
BYTE ReadMEM(BYTE *a, DWORD d, BYTE s) {
  DWORD ad = d & 0x00FFFFFF;
  BYTE sc = (BYTE) (ad >> 16);
  WORD dw = (WORD) (ad & 0x00FFFF);

  Chipset.Cpu.lastMEM = ad;			// address of last access for error
  Chipset.Cpu.lastRW = 0x10;			// read access	

  if ((s > 1) && (ad & 0x00000001))		// odd access as word
    return ADDRESS_ERROR;

  a += s;
  if (ad <= Chipset.RomSize-s) {		// ROM
    while (s) {
      *(--a) = Chipset.Rom[ad++]; s--;
    }
    return BUS_OK;
  } else if ((ad >= Chipset.RamStart) & (ad <= (unsigned)(0x01000000-s))) { // RAM
    ad -= Chipset.RamStart;
    while (s) {
      *(--a) = Chipset.Ram[ad++]; s--;
    }
    return BUS_OK;
  }
  // internal I/O from $400000 to $5FFFFF
  else {
    switch (sc) {
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
      return BUS_ERROR;
      break;
    case 0x42:					        // Keyboard			SC 2
      return Read_Keyboard(a, dw, s);
      break;
    case 0x44:						// Internal disc 9130
      return BUS_ERROR;
      break;
    case 0x47:						// Internal HPIB ($478000)	SC 7
      return Read_HPIB(a, dw, s);
      break;
    case 0x51:						// Display alpha		SC 1
      return readDisplay(a, dw, s);
      break;
    case 0x52:						// Display graph 36c
      return BUS_ERROR;
      break;
    case 0x53:						// Display graph 16		
      return readGraph(a, dw, s);
      break;
    case 0x54:						// Display graph 36c
    case 0x55:
      return BUS_ERROR;
      break;
    case 0x56:
      return BUS_ERROR;
      break;
    case 0x5C:						// 98635 float card
      if (Chipset.Hp98635) return Read_98635(a, dw, s);
      else    	           return BUS_ERROR;
      break;
    case 0x5F:						// ID PROM & SWITCH 
      return BUS_ERROR;
      break;
      // external I/O
    case 0x69:						// 216/217 rs-232 98626		SC 9
      return Read_98626(a, dw, s);
      break;
    default:
      return BUS_ERROR;
      break;
    }
  }
  return BUS_ERROR;					// nothing mapped
}

//
// write at address d from *a with s byte size
//

BYTE WriteMEM(BYTE *a, DWORD d, BYTE s) {
  DWORD ad = d & 0x00FFFFFF;
  BYTE  sc = (BYTE) (ad >> 16);
  WORD  dw = (WORD) (ad & 0x00FFFF);

  Chipset.Cpu.lastMEM = ad;			// address of last access for error
  Chipset.Cpu.lastRW = 0x00;			// write access			

  if ((s > 1) && (ad & 0x00000001))		// odd access as word
    return ADDRESS_ERROR;

  a += s;
  if (ad <= (unsigned)(0x002FFFFF-s)) {		// ROM (up to $2FFFFF) for 9837 ...
    while (s) {
      if (s & 0x01) {
	Chipset.annun &= 0x0007FF;
	Chipset.annun |= (~(*(--a))) << 11;	// low byte is diagnostic leds
      }
      s--;
    }
    return BUS_OK;				// do nothing but no error
  } else if ((ad >= Chipset.RamStart) & (ad <= (unsigned)(0x01000000-s))) {	// RAM (from $880000)
    ad -= Chipset.RamStart;
    while (s) {
      Chipset.Ram[ad++] = *(--a); s--;
    } 
    return BUS_OK;
  }
  // internal I/O from $400000 to $5FFFFF
  else {
    switch (sc) {
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
      return BUS_ERROR;
      break;
    case 0x42:						// Keyboard			SC 2
      return Write_Keyboard(a, dw, s);
      break;
    case 0x44:						// Internal disc
      break;
    case 0x47:						// Internal HPIB ($478000)	SC 7
      return Write_HPIB(a, dw, s);
      break;
    case 0x51:						// Display			SC 1
      return writeDisplay(a, dw, s);
      break;
    case 0x52:						// Display graph 36c
      return BUS_ERROR;
      break;
    case 0x53:						// Display graph 16		
      return writeGraph(a, dw, s);
      break;
    case 0x54:						// Display graph 36c
    case 0x55:
      return BUS_ERROR;
      break;
    case 0x56:
      return BUS_ERROR;
      break;
    case 0x5C:						// 98635 float card
      if (Chipset.Hp98635) 	return Write_98635(a, dw, s);
      else			return BUS_ERROR;
      break;
      // external I/O
    case 0x69: 						// 216/217 rs-232 98626		SC 9
      return Write_98626(a, dw, s);
      break;
    default:
      return BUS_ERROR;
      break;
    }
  }		
  return BUS_ERROR;					// nothing mapped
}

