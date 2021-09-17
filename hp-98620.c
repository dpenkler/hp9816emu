/*
 *   Hp-98620.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   2 channel dma for HPIB IO at 0x500000 to 0x507FF 
//	 non functional, to be done later

#include "common.h"
#include "hp9816emu.h"
#include "kml.h"
#include "mops.h"								// I/O definitions

#define DEBUG_98620						// debug flag

#if defined DEBUG_98620
static	TCHAR buffer[256];
static	int k;
#endif

// hp98626 controller variables

static WORD st98620 = 0;								// state of hp98620 controller

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
// write in DMA IO space
// 

// static DWORD ticks = 0;

static WORD count0;
static WORD count1;

static DWORD address0;
static DWORD address1;

static BYTE cmd0;
static BYTE cmd1;

BYTE Write_98620(BYTE *a, WORD d, BYTE s) {
  if (s != 2) {
    fprintf(stderr,"98620 write access not in word !!");
    return BUS_ERROR;
  }
  if (s == 2) {
    switch (d) {
    case 0x0000:				// 	load address0 high
      address0 = (*(--a) << 24);
      address0 |= (*(--a) << 16);
      return BUS_OK;
      break;
    case 0x0002:				// 	load address0 low
      address0 |= (*(--a) << 8);
      address0 |= *(--a);
      return BUS_OK;
      break;
    case 0x0004:				// 	load count0 
      count0 = (*(--a) << 8);
      count0 |= *(--a);
      return BUS_OK;
      break;
    case 0x0006:				// 	load command0 and interrupt level
      --a;
      cmd0 = *(--a);
      return BUS_OK;
      break;
    case 0x0008:				// 	load address1 high
      address1 = (*(--a) << 24);
      address1 |= (*(--a) << 16);
      return BUS_OK;
      break;
    case 0x000A:				// 	load address1 low
      address1 |= (*(--a) << 8);
      address1 |= *(--a);
      return BUS_OK;
      break;
    case 0x000C:				// 	load count1 
      count1 = (*(--a) << 8);
      count1 |= *(--a);
      return BUS_OK;
      break;
    case 0x000E:				// 	load command1 and interrupt level
      --a;
      cmd1 = *(--a);
      return BUS_OK;
      break;
    }
    return BUS_OK;
  } else return BUS_ERROR;
}

//
// read in DMA IO space
//
BYTE Read_98620(BYTE *a, WORD d, BYTE s)
{
  if (s != 2) {
    fprintf(stderr,"98620 read acces not in word !!");
    return BUS_ERROR;
  }
  if (s == 2) {
    switch (d) {
    case 0x0000:		// Clear 0
      return BUS_OK;
      break;
    case 0x0002:		// nothing
      return BUS_OK;
      break;
    case 0x0004:		// read count0
      return BUS_OK;
      break;
    case 0x0006:		// read status0
      return BUS_OK;
      break;
    case 0x0008:		// Clear 1
      return BUS_OK;
      break;
    case 0x000A:		// nothing
      return BUS_OK;
      break;
    case 0x000C:		// read count1
      return BUS_OK;
      break;
    case 0x000E:		// read status1
      return BUS_OK;
      break;
    }
    return BUS_OK;
  } else return BUS_ERROR;
}

