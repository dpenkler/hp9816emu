/*
 *   Hp-ib.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   HP-IB controller as ADDRESS 21 with TI9114A for 9816 as internal 
//   be carefull some hack for status registers at 0x478003 and 0x478005
//   
//   interrupt at level 3
//
//   quick ack for parallel poll
// 
//	be careful between D0 - D7 from TI9914A docs and I/O data from schematics (D0 - I/O7 .... D7 - I/O0)
//	AND endianness of intel see bit structs in 98x6.h
//
//  address 1 is for system printer in pascal ...   
//
//  just get and send bytes from stacks 
//  but need to implement 3wire handshake protocol
//  pascal HPIB code abort multi sectors transfert sometimes very abruptly, so it was needed
//

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"								// I/O definitions

//#define DEBUG_HPIB
//#define DEBUG_HPIBS
//#define DEBUG_HIGH
#if defined(DEBUG_HPIB) || defined(DEBUG_HPIBS)
static TCHAR buffer[256];
static int k;
//		unsigned long l;
#endif


int bDebugOn=1;
  
#if defined(DEBUG_HPIB) || defined(DEBUG_HPIBS)
static TCHAR *HPIB_CMD[] = {
  _T(""),_T("GTL"),_T(""),_T(""),_T("SDC"),_T("PPC"),_T(""),_T(""),_T("GET"),_T("TCT"),
  _T(""),_T(""),_T(""),_T(""),_T(""),_T(""),_T(""),_T("LLO"),_T(""),_T(""),
  _T("DCL"),_T("PPU"),_T(""),_T(""),_T("SPE"),_T("SPD"),_T(""),_T(""),_T(""),_T(""),
  _T(""),_T(""),
  _T("MLA0"),_T("MLA1"),_T("MLA2"),_T("MLA3"),_T("MLA4"),_T("MLA5"),_T("MLA6"),_T("MLA7"),_T("MLA8"),_T("MLA9"),
  _T("MLA10"),_T("MLA11"),_T("MLA12"),_T("MLA13"),_T("MLA14"),_T("MLA15"),_T("MLA16"),_T("MLA17"),_T("MLA18"),_T("MLA19"),
  _T("MLA20"),_T("MLA21"),_T("MLA22"),_T("MLA23"),_T("MLA24"),_T("MLA25"),_T("MLA26"),_T("MLA27"),_T("MLA28"),_T("MLA29"),
  _T("MLA30"),_T("UNL"),
  _T("MTA0"),_T("MTA1"),_T("MTA2"),_T("MTA3"),_T("MTA4"),_T("MTA5"),_T("MTA6"),_T("MTA7"),_T("MTA8"),_T("MTA9"),
  _T("MTA10"),_T("MTA11"),_T("MTA12"),_T("MTA13"),_T("MTA14"),_T("MTA15"),_T("MTA16"),_T("MTA17"),_T("MTA18"),_T("MTA19"),
  _T("MTA20"),_T("MTA21"),_T("MTA22"),_T("MTA23"),_T("MTA24"),_T("MTA25"),_T("MTA26"),_T("MTA27"),_T("MTA28"),_T("MTA29"),
  _T("MTA30"),_T("UNT"),
  _T("MSA0"),_T("MSA1"),_T("MSA2"),_T("MSA3"),_T("MSA4"),_T("MSA5"),_T("MSA6"),_T("MSA7"),_T("MSA8"),_T("MSA9"),
  _T("MSA10"),_T("MSA11"),_T("MSA12"),_T("MSA13"),_T("MSA14"),_T("MSA15"),_T("MSA16"),_T("MSA17"),_T("MSA18"),_T("MSA19"),
  _T("MSA20"),_T("MSA21"),_T("MSA22"),_T("MSA23"),_T("MSA24"),_T("MSA25"),_T("MSA26"),_T("MSA27"),_T("MSA28"),_T("MSA29"),
  _T("MSA30"),_T("")
};

static TCHAR *HPIB_9114[] = {
  _T("clear swrst"), _T("clear dacr"), _T("rhdf"), _T("clear hdfa"),
  _T("clear hdfe"), _T("nbaf"), _T("clear fget"), _T("clear rtl"),
  _T("feoi"), _T("clear lon"), _T("clear ton"), _T("gts"),
  _T("tca"), _T("tcs"), _T("clear rpp"), _T("clear sic"),
  _T("clear sre"), _T("rqc"), _T("rlc"), _T("clear dai"),
  _T("pts"), _T("clear stdl"), _T("clear shdw"), _T("clear vstdl"),
  _T("clear rsv2"), _T("clear 19"), _T("clear 1A"), _T("clear 1B"),
  _T("clear 1C"), _T("clear 1D"), _T("clear 1E"), _T("clear 1F"),

  _T("set swrst"), _T("set dacr"), _T("rhdf"), _T("set hdfa"),
  _T("set hdfe"), _T("nbaf"), _T("set fget"), _T("set rtl"),
  _T("feoi"), _T("set lon"), _T("set ton"), _T("gts"),
  _T("tca"), _T("tcs"), _T("set rpp"), _T("set sic"),
  _T("set sre"), _T("rqc"), _T("rlc"), _T("set dai"),
  _T("pts"), _T("set stdl"), _T("set shdw"), _T("set vstdl"),
  _T("set rsv2"), _T("set 19"), _T("set 1A"), _T("set 1B"),
  _T("set 1C"), _T("set 1D"), _T("set 1E"), _T("set 1F")
};
#endif

static CHAR *hpib_name[] = { "None ", "9121D", "9122D", "7908", "7911", "7912" };

static WORD sthpib = 0;						// state for hpib state machine

static BYTE key_int = 0;					// keyboard want NMI

//
// receive byte from peripheral
// only for peripherals -> controller
// data byte transaction state machine due to 3 wires protocol
// source part (from peripherals)
BOOL h_push(BYTE b, BYTE st)				// push on data out and status st, 0x80 : EOI
{
  // step 5 : 0 1 1, now the controller should clear nrfd for the next transaction -> 0 0 1
  // step 4 : 0 1 0, now the controller should set ndac for the next cycle -> 0 1 1
  // step 3 : 1 1 0, the source should clear dav to invalidate the data -> 0 1 0
  if (Chipset.Hpib.l_dav && Chipset.Hpib.l_nrfd && !Chipset.Hpib.l_ndac) {	// data valid, not ready for byte, data accepted
    Chipset.Hpib.l_dav = 0;							// data no more valid
    return TRUE;								// ok transaction done
  }
  // step 2 : 1 1 1, now the controller should clear ndac if it accepted the byte -> 1 1 0
  if (Chipset.Hpib.l_dav && Chipset.Hpib.l_nrfd && Chipset.Hpib.l_ndac) {	// data valid, not ready for byte, no data accepted
    Chipset.Hpib.data_in = b;							// data accepted
    Chipset.Hpib.l_eoi = (st) ? 1 : 0;
    Chipset.Hpib.end = (Chipset.Hpib.lads) ? Chipset.Hpib.l_eoi : 0;
    Chipset.Hpib.l_ndac = 0;							// data accepted
    Chipset.Hpib.bi = 1;							// byte in
    Chipset.Hpib.data_in_read = 0;						// now should be read
  }
  // step 1 : 1 0 1, now the controller should set nrfd if ready to get it -> 1 1 1
  // step 0 : 0 0 1, source validate dav if all right -> 1 0 1
  if (!Chipset.Hpib.l_dav && !Chipset.Hpib.l_nrfd && Chipset.Hpib.l_ndac) {	// no data valid, ready for byte, no data acccepted
    Chipset.Hpib.l_dav = 1;							// data valid
  }
  return FALSE;
}

//
// component on hp-ib bus for controller
//
typedef struct
{
  VOID	*send_d;
  VOID	*send_c;
  VOID	*init;
  VOID	*stop;
  VOID	*ctrl;						// is ctrl addresse
  BOOL	done;						// data send with success
} HPIB_INST;

//
// define the actual instruments used
//
static HPIB_INST hpib_bus[8];		// max 8 instruments

//
//	Draw state and name of instruments on right hand panel
//
VOID hpib_names(VOID) {
  Chipset.annun &= ~(1 << 1);
  if (Chipset.Hpib70x == 1) {
    emuUpdateDisk(0, hpib_name[1]);
    if (Chipset.Hp9121.lifname[0][0] != 0x00)
      emuUpdateButton(Chipset.Hp9121.hpibaddr, 0, Chipset.Hp9121.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp9121.hpibaddr, 0, "");
    if (Chipset.Hp9121.lifname[1][0] != 0x00)
      emuUpdateButton(Chipset.Hp9121.hpibaddr, 1, Chipset.Hp9121.lifname[1]);
    else
      emuUpdateButton(Chipset.Hp9121.hpibaddr, 1, "");
    Chipset.annun |= (1 << 1);
  } else {
    emuUpdateDisk(0, hpib_name[0]);
    emuUpdateButton(Chipset.Hp9121.hpibaddr, 0, "");
    emuUpdateButton(Chipset.Hp9121.hpibaddr, 1, "");
  }
  
  Chipset.annun &= ~(1 << 4);
  if (Chipset.Hpib72x == 3) {
    if (Chipset.Hp9122.lifname[0][0] != 0x00)
      emuUpdateButton(Chipset.Hp9122.hpibaddr, 0, Chipset.Hp9122.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp9122.hpibaddr, 0, "");
    if (Chipset.Hp9122.lifname[1][0] != 0x00)
      emuUpdateButton(Chipset.Hp9122.hpibaddr, 1, Chipset.Hp9122.lifname[1]);
    else
      emuUpdateButton(Chipset.Hp9122.hpibaddr, 1, "");
    Chipset.annun |= (1 << 4);
  } else {
    emuUpdateDisk(1, hpib_name[0]);
    emuUpdateButton(Chipset.Hp9122.hpibaddr, 0, "");
    emuUpdateButton(Chipset.Hp9122.hpibaddr, 1, "");
  }
  
  Chipset.annun &= ~(1 << 7);
  switch (Chipset.Hpib73x) {
  case 1:
    if (Chipset.Hp7908_0.lifname[0][0] == 0x00)
      emuUpdateButton(Chipset.Hp7908_0.hpibaddr, 0, Chipset.Hp7908_0.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp7908_0.hpibaddr, 0, "");
    emuUpdateDisk(2, hpib_name[6]);		// 7908
    Chipset.annun |= (1 << 7);
    break;
  case 2:
    if (Chipset.Hp7908_0.lifname[0][0] == 0x00)
      emuUpdateButton(Chipset.Hp7908_0.hpibaddr, 0, Chipset.Hp7908_0.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp7908_0.hpibaddr, 0,  "");
    emuUpdateDisk(2, hpib_name[7]);		// 7911
    Chipset.annun |= (1 << 7);
    break;
  case 3:
    if (Chipset.Hp7908_0.lifname[0][0] == 0x00)
      emuUpdateButton(Chipset.Hp7908_0.hpibaddr, 0, Chipset.Hp7908_0.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp7908_0.hpibaddr, 0, "");
    emuUpdateDisk(2, hpib_name[8]);		// 7912
    Chipset.annun |= (1 << 7);
    break;
  }
  
  Chipset.annun &= ~(1 << 9);
  switch (Chipset.Hpib74x) {
  case 1:
    if (Chipset.Hp7908_1.lifname[0][0] == 0x00)
      emuUpdateButton(Chipset.Hp7908_1.hpibaddr, 0, Chipset.Hp7908_1.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp7908_1.hpibaddr, 0, "");
    emuUpdateDisk(3, hpib_name[3]);		// 7908
    Chipset.annun |= (1 << 9);
    break;
  case 2:
    if (Chipset.Hp7908_1.lifname[0][0] == 0x00)
      emuUpdateButton(Chipset.Hp7908_1.hpibaddr, 0, Chipset.Hp7908_1.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp7908_1.hpibaddr, 0, "");
    emuUpdateDisk(3, hpib_name[4]);		// 7911
    Chipset.annun |= (1 << 9);
    break;
  case 3:
    if (Chipset.Hp7908_1.lifname[0][0] == 0x00)
      emuUpdateButton(Chipset.Hp7908_1.hpibaddr, 0, Chipset.Hp7908_1.lifname[0]);
    else
      emuUpdateButton(Chipset.Hp7908_1.hpibaddr, 0, "");
    emuUpdateDisk(3, hpib_name[5]);		// 7912
    Chipset.annun |= (1 << 9);
    break;
  default:
    emuUpdateDisk(3, hpib_name[0]);		// 7912 
  }
}

//
// initialize HPIB bus
//
VOID hpib_init(VOID) {
  BYTE i = 0;

  if (Chipset.Hpib71x) {				// add 701 printer ?
    hpib_bus[i].send_d = hp2225_push_d;
    hpib_bus[i].send_c = hp2225_push_c;
    hpib_bus[i].init   = hp2225_reset;
    hpib_bus[i].stop   = hp2225_stop;
    hpib_bus[i++].ctrl = (VOID *) (&Chipset.Hp2225);
    Chipset.Hp2225.hpibaddr = 1;
  }
 if (Chipset.Hpib70x == 1) {				// add 700 unit 9121
    hpib_bus[i].send_d = hp9121_push_d;
    hpib_bus[i].send_c = hp9121_push_c;
    hpib_bus[i].init   = hp9121_reset;
    hpib_bus[i].stop   = hp9121_stop;
    hpib_bus[i++].ctrl = (VOID *) (&Chipset.Hp9121);
    Chipset.Hp9121.hpibaddr = 0;
    Chipset.Hp9121.ctype = 0;
  }
  if (Chipset.Hpib72x == 3) {				// add 702 unit 9122
    hpib_bus[i].send_d = hp9122_push_d;
    hpib_bus[i].send_c = hp9122_push_c;
    hpib_bus[i].init   = hp9122_reset;
    hpib_bus[i].stop   = hp9122_stop;
    hpib_bus[i++].ctrl = (VOID *) (&Chipset.Hp9122);
    Chipset.Hp9122.hpibaddr = 2;
  }
   switch (Chipset.Hpib73x) {				// add 703 unit
  default:
    break;
  case 1:						// hp7908
  case 2:						// hp7911
  case 3:						// hp7912
    hpib_bus[i].send_d = hp7908_push_d;
    hpib_bus[i].send_c = hp7908_push_c;
    hpib_bus[i].init   = hp7908_reset;
    hpib_bus[i].stop   = hp7908_stop;
    hpib_bus[i++].ctrl = (VOID *) (&Chipset.Hp7908_0);
    Chipset.Hp7908_0.hpibaddr = 3;
    Chipset.Hp7908_0.type[0] = Chipset.Hpib73x - 1;
    break;
  }
  switch (Chipset.Hpib74x) {				// add 704 unit
  default:
    break;
  case 1:						// hp7908
  case 2:						// hp7911
  case 3:						// hp7912
    hpib_bus[i].send_d = hp7908_push_d;
    hpib_bus[i].send_c = hp7908_push_c;
    hpib_bus[i].init = hp7908_reset;
    hpib_bus[i].stop = hp7908_stop;
    hpib_bus[i++].ctrl = (VOID *) (&Chipset.Hp7908_1);
    Chipset.Hp7908_1.hpibaddr = 4;
    Chipset.Hp7908_1.type[0] = Chipset.Hpib74x - 1;
    break;
  }
 
  hpib_bus[i].send_d = NULL;				// end of bus
  hpib_bus[i].send_c = NULL;
  hpib_bus[i].init = NULL;
  hpib_bus[i].stop = NULL;
  hpib_bus[i].ctrl = NULL;
}

//
// send data byte to peripherals, no transaction needed, but can failed ...
// success when ALL instruments have accepted the data
//
BOOL hpib_send_d(BYTE d) {
  int i = 0;
  BOOL failed = FALSE;

#if defined DEBUG_HPIBS
#if defined DEBUG_HIGH
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
    k = wsprintf(buffer,_T("%06X: HPIB : send d (%d):%02X\n"), Chipset.Cpu.PC, d, Chipset.Hpib.s_eoi);
    OutputDebugString(buffer);
#if defined DEBUG_HIGH
  }
#endif
#endif

  while (hpib_bus[i].send_d != NULL) {		// some instrument ?
    if (!hpib_bus[i].done) {			// data not already accepted
      if (((BOOL (*)(VOID *, BYTE, BYTE))hpib_bus[i].send_d)(hpib_bus[i].ctrl, d, Chipset.Hpib.s_eoi)) {
	hpib_bus[i].done = TRUE;		// data accepted
      } else {		
	failed = TRUE;				// not accepted
	hpib_bus[i].done = FALSE;		// not accepted
      }
    }
    i++;
  }
  if (failed) {					// failed, leave with FALSE
    return FALSE;
  } else {					// success, re init done part
    i = 0;
    while (hpib_bus[i].send_d != NULL) {
      hpib_bus[i].done = FALSE;
      i++;
    }
    Chipset.Hpib.s_eoi = 0;
    return TRUE;				// success, leave with TRUE
  }
}

//
// send command byte to peripherals, no transaction needed
//
BOOL hpib_send_c(BYTE c) {
  int i = 0;

#if defined DEBUG_HPIBS
#if defined DEBUG_HIGH
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
    k = wsprintf(buffer,_T("%06X: HPIB : send c :%02X (%s)\n"), Chipset.Cpu.PC, c, HPIB_CMD[c & 0x7F]);
    OutputDebugString(buffer);
#if defined DEBUG_HIGH
  }
#endif
#endif

  while (hpib_bus[i].send_c != NULL) {
    ((BOOL (*)(VOID *, BYTE))hpib_bus[i].send_c)(hpib_bus[i].ctrl, c);
    i++;
  }
  return TRUE;
}

//
// initialize all instruments on bus
//
BOOL hpib_init_bus(VOID) {
  int i = 0;

#if defined DEBUG_HPIBS
#if defined DEBUG_HIGH
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
    k = wsprintf(buffer,_T("%06X: HPIB : init bus\n"), Chipset.Cpu.PC);
    OutputDebugString(buffer);
#if defined DEBUG_HIGH
  }
#endif
#endif

  while (hpib_bus[i].init != NULL) {
    hpib_bus[i].done = FALSE;
    ((VOID (*)(VOID *))hpib_bus[i].init)(hpib_bus[i].ctrl);
    i++;
  }
  return TRUE;
}

//
// stop all instruments on bus
//
BOOL hpib_stop_bus(VOID) {
  int i = 0;

#if defined DEBUG_HPIBS
#if defined DEBUG_HIGH
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
    k = wsprintf(buffer,_T("%06X: HPIB : stop bus\n"), Chipset.Cpu.PC);
    OutputDebugString(buffer);
#if defined DEBUG_HIGH
  }
#endif
#endif

  while (hpib_bus[i].stop != NULL) {
    hpib_bus[i].done = FALSE;
    ((VOID (*)(VOID *))hpib_bus[i].stop)(hpib_bus[i].ctrl);
    i++;
  }
  return TRUE;
}

//################
//#
//#    Public functions
//#
//################

//
// write in HPIB I/O space
// dmaen is not used at all
// (see hp-98620.c)
//
BYTE Write_HPIB(BYTE *a, WORD d, BYTE s) {
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
    k = wsprintf(buffer,_T("%06X: HPIB : %02X->%04X\n"), Chipset.Cpu.PC, *(a-1), d);
    OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
  }
#endif
#endif
	
  if (s != 1) {
    fprintf(stderr,"HPIB write acces not in byte !!");
    return BUS_ERROR;
  }

  a--;		// correct the little endianness

  d = (0x8000) | (d & 0x00FF);

  switch (d) {
  case 0x8003:								// special dma enable for HPIB ?
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB DMAen <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.h_dmaen = (*a & 0x01) ? 1 : 0;
    break;

  case 0x8011:								// 9114 reg 0 Interrupt mask 0
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Interrupt mask 0 <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.intmask0 = *a;
    break;
  case 0x8013:								// 9114 reg 1 Interrupt mask 1
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Interrupt mask 1 <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.intmask1 = *a;
    break;
  case 0x8015:										// 9114 reg 2 nothing
    break;
  case 0x8017:										// 9114 reg 3 Auxilliary command
    Chipset.Hpib.aux_cmd = *a;
    if (sthpib != 0) {
      fprintf(stderr,"HPIB command not in idle state !!");
    }
    sthpib = 1;
    break;
  case 0x8019:										// 9114 reg 4 Address
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Address <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.address = (*a) & 0x1F;
    break;
  case 0x801B:										// 9114 reg 5 Serial Poll
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Serial Poll <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.ser_poll = *a;
    break;
  case 0x801D:										// 9114 reg 6 Parallel Poll
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Parallel Poll <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.par_poll = *a;
    break;
  case 0x801F:										// 9114 reg 7 Data out
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Data out <- %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    Chipset.Hpib.data_out = *a;
    Chipset.Hpib.data_out_loaded = 1;
    Chipset.Hpib.bo = 0;						// clear BO
    if (Chipset.Hpib.atn) {
      hpib_send_c(*a);							// send command
      if (((*a) & 0x7F) == 0x20 + Chipset.Hpib.address) {		// listen
	Chipset.Hpib.lads = 1;
      } else if (((*a) & 0x7F) == 0x3f) {				// unlisten all
	Chipset.Hpib.lads = 0;
      }	 else if (((*a) & 0x60) == 0x20) {				// listen other
	Chipset.Hpib.a_lon = 0;
      } else if (((*a) & 0x7F) == 0x40 + Chipset.Hpib.address) {	// talk
	Chipset.Hpib.tads = 1;
      } else if (((*a) & 0x7F) == 0x5F) {				// untalk all
	Chipset.Hpib.tads = 0;
      } else if (((*a) & 0x60) == 0x40) {				// talk other
	Chipset.Hpib.a_ton = 0;
      }
      if (Chipset.Hpib.tads || Chipset.Hpib.h_controller)
	Chipset.Hpib.bo = 1;						// set BO, stuff sended with success
      Chipset.Hpib.data_out_loaded = 0;
    }
    /*			else
			hpib_send_d(*a);								// send data
			if (Chipset.Hpib.tads || Chipset.Hpib.h_controller)
			Chipset.Hpib.bo = 1;								// set BO, stuff sended with success
			Chipset.Hpib.data_out_loaded = 0; */
    break;
  default: 
    return BUS_ERROR;
    break;
  }
  return BUS_OK;
}

//
// read in HPIB I/O space
//
BYTE Read_HPIB(BYTE *a, WORD d, BYTE s) {

#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
    k = wsprintf(buffer,_T("%06X: HPIB : read %04X\n"), Chipset.Cpu.PC, d);
    OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
  }
#endif
#endif

  if (s != 1) {
    fprintf(stderr,"HPIB read acces not in byte !!");
    return BUS_ERROR;
  }

  a--;		// correct the little endianness

  d = (0x8000) | (d & 0x00FF);
  switch (d) {
  case 0x8011:								// 9114 reg 0 Interrupt status 0
    *a = Chipset.Hpib.status0;
    Chipset.Hpib.status0 &= 0xC0;					// clear after read except bi & end
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Interrupt status 0 = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif 
    break;
  case 0x8013:								// 9114 reg 1 Interrupt status 1
    *a = Chipset.Hpib.status1;
    Chipset.Hpib.status1 = 0x00;					// clear after read
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Interrupt status 1 = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif 
    break;
  case 0x8015:								// 9114 reg 2 Address status
    *a = Chipset.Hpib.statusad;
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("%06X: HPIB Address status = %02X\n"), Chipset.Cpu.PC, *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    break;
  case 0x8017:								// 9114 reg 3 Bus status
    *a = Chipset.Hpib.statusbus;
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Bus status = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    break;
  case 0x8019:							// 9114 reg 4 nothing
  case 0x801B:							// 9114 reg 5 nothing
    *a = 0xFF;
    break;
  case 0x801D:							// 9114 reg 6 Command pass thru for parallel poll
    *a = (Chipset.Hpib.a_rpp) ? Chipset.Hpib.par_poll_resp : Chipset.Hpib.data_in;
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Command pass thru = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    break;
  case 0x801F:							// 9114 reg 7 Data in
    *a = Chipset.Hpib.data_in;
    Chipset.Hpib.data_in_read = 1;
    Chipset.Hpib.bi = 0;					// clear BI
    Chipset.Hpib.end = 0;
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB Data in = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    break;

  case 0x8003:							// special status byte 0
    *a = Chipset.Hpib.h_dmaen | (Chipset.Hpib.h_int << 6) | ((Chipset.switch1 & 0x20) << 2) | 0x04;		// 0x04 for 9816
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB spec status 1 = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    break;
  case 0x8005:							// special status byte 1
    key_int = (Chipset.Keyboard.int68000 == 7);			// did keyboard wants an NMI ?
    *a = ((Chipset.switch1 & 0x80) >> 7) | (key_int << 2) | ((!Chipset.Hpib.h_controller) << 6) | ((Chipset.switch1 & 0x40) << 1);
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
      k = wsprintf(buffer,_T("	    : HPIB spec status 5 = %02X\n"), *a);
      OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
    }
#endif
#endif
    break;

  default: 
    return BUS_ERROR;
    break;
  }
  return BUS_OK;
}

//
// state machine for TI9914A controller
//
VOID DoHPIB(VOID) {
  BOOL sc;
  //	BYTE st;

  WORD mem_rem = Chipset.Hpib.rem;

  // data byte transaction state machine due to 3 wire protocol
  // acceptor part (from controller)
  // step 5 : 0 1 1, now the controller should clear nrfd for the next transaction -> 0 0 1
  if (!Chipset.Hpib.l_dav && Chipset.Hpib.l_nrfd && Chipset.Hpib.l_ndac) {	// no data valid, not ready for byte, no data accepted
    // RFD holdoff
    if (Chipset.Hpib.a_hdfa || (Chipset.Hpib.a_hdfe && Chipset.Hpib.end)) {
      // rfd holdoff
    } else if (Chipset.Hpib.data_in_read)
      Chipset.Hpib.l_nrfd = 0;							// ready for byte
  }
  // step 4 : 0 1 0, now the controller should set ndac for the next cycle -> 0 1 1
  if (!Chipset.Hpib.l_dav && Chipset.Hpib.l_nrfd && !Chipset.Hpib.l_ndac) {	// no data valid, not ready for byte, data accepted
    Chipset.Hpib.l_ndac = 1;							// no more data accepted
  }
  // step 3 : 1 1 0, the source should clear dav to invalidate the data -> 0 1 0
  // step 2 : 1 1 1, now the controller should clear ndac if it accepted the byte -> 1 1 0
  if (Chipset.Hpib.l_dav && Chipset.Hpib.l_nrfd && Chipset.Hpib.l_ndac) {	// data valid, not ready for byte, no data accepted
    //		Chipset.Hpib.l_ndac = 0;					// data accepted
    //		Chipset.Hpib.bi = 1;						// byte in
  }
  // step 1 : 1 0 1, now the controller should set nrfd if ready to get it -> 1 1 1
  if (Chipset.Hpib.l_dav && !Chipset.Hpib.l_nrfd && Chipset.Hpib.l_ndac) {	// data valid, ready for byte, no data accepted
    if (Chipset.Hpib.data_in_read) 
      Chipset.Hpib.l_nrfd = 1;							// not ready for byte
  }
  // step 0 : 0 0 1, source validate dav if all right -> 1 0 1

  // send data if needed
  if (Chipset.Hpib.data_out_loaded) {						// ok try to send ...
    if (hpib_send_d(Chipset.Hpib.data_out)) {					// success
      if (Chipset.Hpib.tads || Chipset.Hpib.h_controller)
	Chipset.Hpib.bo = 1;							// set BO, stuff sended with success
      Chipset.Hpib.data_out_loaded = 0;
    }
  }

  switch (sthpib)
    {
    case 0:									// IDLE STATE, wait
      break;
    case 1:									// do command
      sc = (Chipset.Hpib.aux_cmd & 0x80) ? 1 : 0;				// set or clear ?
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
	k = wsprintf(buffer,_T("      : HPIB auxiliary : %02X = %s\n"), Chipset.Hpib.aux_cmd, HPIB_9114[((sc) ? 32 : 0) + (Chipset.Hpib.aux_cmd & 0x1F)]);
	OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
      }
#endif
#endif
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
	k = wsprintf(buffer,_T("      : HPIB auxiliary : --- : dav %d nrfd %d ndac %d bi %d\n"), Chipset.Hpib.l_dav, Chipset.Hpib.l_nrfd, Chipset.Hpib.l_ndac, Chipset.Hpib.bi);
	OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
      }
#endif
#endif
      Chipset.Hpib.gts = 0;
      switch(Chipset.Hpib.aux_cmd & 0x1F) {
      case 0x00:							// swrst (cs) software reset
	Chipset.Hpib.a_swrst = sc;
	if (Chipset.Hpib.a_swrst) {
	  //						h_out_hi = 0;				
	  //						h_out_lo = 0;
	  Chipset.Hpib.h_out_hi = 0;					// hi pointer on bus				
	  Chipset.Hpib.h_out_lo = 0;
	  Chipset.Hpib.h_dmaen = 0;					// dma enable for hpib
	  Chipset.Hpib.h_controller = 0;				// hpib controller is in control ?
	  Chipset.Hpib.h_sysctl = 1;					// hpib is system controller by default
	  Chipset.Hpib.h_int = 0;					// hpib want interrupt
	  Chipset.Hpib.data_in = 0;
	  Chipset.Hpib.data_in_read= 1;					// data_in read, can load the next
	  Chipset.Hpib.aux_cmd= 0;
	  Chipset.Hpib.address = 0;
	  Chipset.Hpib.ser_poll = 0;
	  Chipset.Hpib.par_poll = 0;
	  Chipset.Hpib.par_poll_resp = 0;				// par poll response
	  Chipset.Hpib.data_out = 0;
	  Chipset.Hpib.data_out_loaded = 0;
	  Chipset.Hpib.status0 = 0x00;
	  Chipset.Hpib.status1 = 0x00;
	  Chipset.Hpib.statusad = 0;
	  Chipset.Hpib.statusbus = 0x00;
	  Chipset.Hpib.intmask0 = 0x00;
	  Chipset.Hpib.intmask1 = 0x00;
	  Chipset.Hpib.l_atn = 0;
	  Chipset.Hpib.l_eoi = 0;
	  Chipset.Hpib.l_dav = 0;
	  Chipset.Hpib.l_ifc = 0;
	  Chipset.Hpib.l_ndac = 1;					// for starting handshake
	  Chipset.Hpib.l_nrfd = 0;
	  Chipset.Hpib.l_ren = 0;
	  Chipset.Hpib.l_srq = 0;
	  Chipset.Hpib.a_swrst = 0;
	  Chipset.Hpib.a_dacr = 0;
	  Chipset.Hpib.a_hdfa = 0;
	  Chipset.Hpib.a_hdfe = 0;
	  Chipset.Hpib.a_fget = 0;
	  Chipset.Hpib.a_rtl = 0;
	  Chipset.Hpib.a_lon = 0;
	  Chipset.Hpib.a_ton = 0;
	  Chipset.Hpib.a_rpp = 0;
	  Chipset.Hpib.a_sic = 0;
	  Chipset.Hpib.a_sre = 0;
	  Chipset.Hpib.a_dai = 0;					// 9914A diseable all interrupt
	  Chipset.Hpib.a_stdl = 0;
	  Chipset.Hpib.a_shdw = 0;
	  Chipset.Hpib.a_vstdl = 0;
	  Chipset.Hpib.a_rsv2 = 0;
	  Chipset.Hpib.s_eoi = 0;					// 9914A send oei with next byte
	  // Chipset.Hpib.bo = 1;									
	}
	sthpib = 0;
	break;
      case 0x01:							// dacr (cs) release DAC holdoff
	Chipset.Hpib.a_dacr = sc;
	sthpib = 0;
	break;
      case 0x02:							// rhdf (--) release RFD holdoff
	Chipset.Hpib.l_nrfd = 0;
	sthpib = 0;
	break;
      case 0x03:							// hdfa (cs) hold off on all data
	Chipset.Hpib.a_hdfa = sc;
	sthpib = 0;
	break;
      case 0x04:							// hdfe (cs) hold off on EOI only
	Chipset.Hpib.a_hdfe = sc;
	sthpib = 0;
	break;
      case 0x05:							// nbaf (--) new byte available false
	sthpib = 0;
	break;
      case 0x06:							// fget (cs) force group execution trigger
	Chipset.Hpib.a_fget = sc;
	sthpib = 0;
	break;
      case 0x07:							// rtl (cs) return to local
	Chipset.Hpib.a_rtl = sc;
	sthpib = 0;
	break;
      case 0x08:							// feoi (--) send EOI with next byte
	Chipset.Hpib.s_eoi = sc;
	sthpib = 0;
	break;
      case 0x09:							// lon (cs) listen only
	//					if ((!Chipset.Hpib.lads) && sc) 
	//						hpib_send_c(0x20 | Chipset.Hpib.address);
	Chipset.Hpib.a_lon = sc;
	Chipset.Hpib.llo = 0;
	if (sc) Chipset.Hpib.rem = 1;			// do rem
	Chipset.Hpib.lads = sc;					// do lads
	if (sc) Chipset.Hpib.tads = 0;
	sthpib = 0;
	break;
      case 0x0A:							// ton (cs) talk only
	//					if ((!Chipset.Hpib.tads) && sc)
	//						hpib_send_c(0x40 | Chipset.Hpib.address);
	Chipset.Hpib.a_ton = sc;
	Chipset.Hpib.rem = 0;
	Chipset.Hpib.llo = 0;
	Chipset.Hpib.tads = sc;
	if (sc) Chipset.Hpib.lads = 0;
	sthpib = 0;
	break;
      case 0x0B:							// gts (--) go to standby
	Chipset.Hpib.l_atn = 0;
	Chipset.Hpib.gts = 1;
	if (Chipset.Hpib.h_controller || Chipset.Hpib.tads)
	  Chipset.Hpib.bo = 1;
	sthpib = 0;
	break;
      case 0x0C:							// tca (--) take control asynchronously
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
	  k = wsprintf(buffer,_T("	: HPIB auxiliary : tca : dav %d nrfd %d ndac %d bi %d\n"), Chipset.Hpib.l_dav, Chipset.Hpib.l_nrfd, Chipset.Hpib.l_ndac, Chipset.Hpib.bi);
	  OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
	}
#endif
#endif
	Chipset.Hpib.l_dav = 0;
	Chipset.Hpib.l_nrfd = 0; 
	Chipset.Hpib.l_ndac = 1;		                         // take back synchronous control
	Chipset.Hpib.l_atn = 1;
	Chipset.Hpib.bi = 0;
	Chipset.Hpib.data_in_read = 1;
	Chipset.Hpib.bo = 1;
	sthpib = 0;
	break;
      case 0x0D:							// tcs (--) take control synchronously
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
	  k = wsprintf(buffer,_T("	: HPIB auxiliary : tcs : dav %d nrfd %d ndac %d bi %d\n"), Chipset.Hpib.l_dav, Chipset.Hpib.l_nrfd, Chipset.Hpib.l_ndac, Chipset.Hpib.bi);
	  OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
	}
#endif
#endif
	//					Chipset.Hpib.l_dav = 0;
	//					Chipset.Hpib.l_nrfd = 0; 
	//					Chipset.Hpib.l_ndac = 1;		// take back synchronous control
	Chipset.Hpib.l_atn = 1;
	Chipset.Hpib.bi = 0;
	Chipset.Hpib.data_in_read = 1;
	Chipset.Hpib.bo = 1;
	sthpib = 0;
	break;
      case 0x0E:							// rpp (cs) request parallel poll
	Chipset.Hpib.a_rpp = sc;
	sthpib = 0;
	break;
      case 0x0F:							// sic (cs) send interface clear
	Chipset.Hpib.a_sic = sc;
	Chipset.Hpib.ifc = sc;
	if (sc) Chipset.Hpib.h_controller = 1;				// take control
	sthpib = 0;
	break;
      case 0x10:							// sre (cs) send remote enable
	Chipset.Hpib.a_sre = sc;
	Chipset.Hpib.l_ren = sc;
	sthpib = 0;
	break;
      case 0x11:							// rqc (--) request control
	// Chipset.Hpib.h_controller = 1;
	sthpib = 0;
	break;
      case 0x12:							// rlc (--) release control
	Chipset.Hpib.l_atn = 0;
	sthpib = 0;
	break;
      case 0x13:							// dai (cs) disable all interrupts
	Chipset.Hpib.a_dai = sc;
	sthpib = 0;
	break;
      case 0x14:							// pts (--) pass through next secondary
	sthpib = 0;
	break;
      case 0x15:							// stdl (cs) short TI settling time
	Chipset.Hpib.a_stdl = sc;
	sthpib = 0;
	break;
      case 0x16:							// shdw (cs) shadow handshake
	Chipset.Hpib.a_shdw = sc;
	if (Chipset.Hpib.a_shdw)
	  fprintf(stderr,"HPIB shadow handshake !!");
	sthpib = 0;
	break;
      case 0x17:							// vstdl (cs) very short T1 delay
	Chipset.Hpib.a_vstdl = sc;
	sthpib = 0;
	break;
      case 0x18:							// rsv2 (cs) request service bit 2
	Chipset.Hpib.a_rsv2 = sc;
	sthpib = 0;
	break;
      default:
	sthpib = 0;
	break;
      }
#if defined DEBUG_HPIB
#if defined DEBUG_HIGH
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
#endif
	k = wsprintf(buffer,_T("      : HPIB auxiliary : --- : dav %d nrfd %d ndac %d bi %d data %02X\n"), Chipset.Hpib.l_dav, Chipset.Hpib.l_nrfd, Chipset.Hpib.l_ndac, Chipset.Hpib.bi, Chipset.Hpib.data_in);
	OutputDebugString(buffer); buffer[0] = 0x00;
#if defined DEBUG_HIGH
      }
#endif
#endif
    }
  Chipset.Hpib.atn = Chipset.Hpib.l_atn;
  Chipset.Hpib.ifc = (Chipset.Hpib.h_sysctl) ? 0 : Chipset.Hpib.l_ifc;	// not set when system controller

  if (mem_rem ^ Chipset.Hpib.rem)		                	// RLC ?
    Chipset.Hpib.rlc = 1;						// set it
  //	if (!Chipset.Hpib.data_out_loaded && (Chipset.Hpib.h_controller || (Chipset.Hpib.tads)))	// BO ?
  //		Chipset.Hpib.bo = 1;					                                // set it

  // do INT0
  if (Chipset.Hpib.intmask0 & Chipset.Hpib.int0 & 0x3F) Chipset.Hpib.int0 = 1;
  else Chipset.Hpib.int0 = 0;
  // do INT1
  if (Chipset.Hpib.intmask1 & Chipset.Hpib.int1) Chipset.Hpib.int1 = 1;
  else Chipset.Hpib.int1 = 0;

  Chipset.Hpib.h_int = (!Chipset.Hpib.a_dai) && (Chipset.Hpib.int1 || Chipset.Hpib.int0);

  // hack for // poll

  if (Chipset.Hpib.a_rpp) {
    Chipset.Hpib.par_poll_resp = 0;
    if (Chipset.Hpib70x == 1) {
      if (Chipset.Hp9121.ppol_e) 
	Chipset.Hpib.par_poll_resp |= 0x80;
    }
    if (Chipset.Hpib72x == 3) {
      if (Chipset.Hp9122.ppol_e) 
	Chipset.Hpib.par_poll_resp |= 0x20;
    }
    switch (Chipset.Hpib73x) {
    case 1:
    case 2:
    case 3:
      if (Chipset.Hp7908_0.ppol_e) 
	Chipset.Hpib.par_poll_resp |= 0x10;
      break;
    default:
      break;
    }
    switch (Chipset.Hpib74x) {
    case 1:
    case 2:
    case 3:
      if (Chipset.Hp7908_1.ppol_e) 
	Chipset.Hpib.par_poll_resp |= 0x08;
      break;
    default:
      break;
    }
  }

}
