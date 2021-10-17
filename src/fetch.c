/*
 *   Fetch.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   Opcode fetch and execute dispatcher
//

#include "common.h"
#include "hp9816emu.h"
#include "mops.h"

// #define DEBUG_FETCH
#if defined DEBUG_FETCH
	TCHAR buffer[256];
	int k;
	unsigned long l;
#endif

static WORD byLastOp;

// static table for 68000 opcodes (a little bit faster this way)
static OP opes[65536];

//
// Calc ea.addr with cycles
//
BYTE calc_ea(EA *ea) {
  BYTE status;
  MEM offset;
  EXTW ext;

  status = BUS_OK;
  ea->addr.l = 0x00000000;
  switch (ea->imode) {
  case EA_NONE:
  case EA_CST:
  case EA_D:								// direct Dn
  case EA_A:								// direct An
    break;
  case EA_IA:								// indirect (An)
  case EA_IAI:								// indirect (An)+
    if (Chipset.Cpu.SR.S && (ea->reg == 7)) ea->addr.l = Chipset.Cpu.A[8].l;
    else ea->addr.l = Chipset.Cpu.A[ea->reg].l;
    break;
  case EA_IAD:								// indirect -(An)
    if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
      ea->addr.l = Chipset.Cpu.A[8].l - ((ea->isize == 4) ? 4 : 2);	// force word align on SSP
    } else {
      if (ea->reg == 7) ea->addr.l = Chipset.Cpu.A[7].l - ((ea->isize == 4) ? 4 : 2);	// force word align on USP
      else ea->addr.l = Chipset.Cpu.A[ea->reg].l - ea->isize;
    }
    Chipset.dcycles += 2; // not always, depend on instruction, corrected in instruction
    break;
  case EA_IDA:								// indirect (d16,An)
    if (Chipset.Cpu.SR.S && (ea->reg == 7))
      ea->addr.l = Chipset.Cpu.A[8].l;
    else
      ea->addr.l = Chipset.Cpu.A[ea->reg].l;
    status = ReadMEM((BYTE *) & offset.w[0], Chipset.Cpu.PC, 2);
    Chipset.dcycles += 4;
    if (status == BUS_OK) {
      Chipset.Cpu.PC += 2;
      offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;		// sign extension
      ea->addr.l += offset.l;
    }
    break;
  case EA_IDAX:								// indirect (d8, Xn.s, An)
    if (Chipset.Cpu.SR.S && (ea->reg == 7))
      ea->addr.l = Chipset.Cpu.A[8].l;
    else
      ea->addr.l = Chipset.Cpu.A[ea->reg].l;
    status = ReadMEM((BYTE *) & ext.w, Chipset.Cpu.PC, 2);
    Chipset.dcycles += 4;
    if (status == BUS_OK) {
      Chipset.Cpu.PC += 2;
      offset.b[0] = ext.d8;
      offset.b[1] = (ext.d8 & 0x80) ? 0xFF : 0x00;			// sign extension
      offset.w[1] = (ext.d8 & 0x80) ? 0xFFFF : 0x0000;			// sign extension
      ea->addr.l += offset.l;
      if (ext.da) {							// index is An
	if (ext.wl) {							// An.L
	  if ((ext.reg == 7) && Chipset.Cpu.SR.S) offset.l = Chipset.Cpu.A[8].l;
	  else offset.l = Chipset.Cpu.A[ext.reg].l;
	} else {							// An.W
	  if ((ext.reg == 7) && Chipset.Cpu.SR.S) offset.w[0] = Chipset.Cpu.A[8].w[0];
	  else offset.w[0] = Chipset.Cpu.A[ext.reg].w[0];
	  offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	}
      } else {								// index is Rn
	if (ext.wl) {							// Dn.L
	  offset.l = Chipset.Cpu.D[ext.reg].l;
	} else {							// Dn.W
	  offset.w[0] = Chipset.Cpu.D[ext.reg].w[0];
	  offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;	// sign extension
	}
      }
      ea->addr.l += offset.l;
      Chipset.dcycles += 2;
    }
    break;
  case EA_IXW:
    status = ReadMEM((BYTE *) & ea->addr.w[0], Chipset.Cpu.PC, 2);
    Chipset.dcycles += 4;
    if (status == BUS_OK) {
      Chipset.Cpu.PC += 2;
      ea->addr.w[1] = (ea->addr.w[0] & 0x8000) ? 0xFFFF : 0x0000;	// sign extension
    }
    break;
  case EA_IXL:
    status = ReadMEM((BYTE *) & ea->addr.l, Chipset.Cpu.PC, 4);
    Chipset.dcycles += 8;
    if (status == BUS_OK) {
      Chipset.Cpu.PC += 4;
    }
    break;
  case EA_IDP:								// indirect (d16,PC)
    ea->addr.l = Chipset.Cpu.PC;
    status = ReadMEM((BYTE *) & offset.w[0], Chipset.Cpu.PC, 2);
    Chipset.dcycles += 4;
    if (status == BUS_OK) {
      Chipset.Cpu.PC += 2;
      offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;		// sign extension
      ea->addr.l += offset.l;
    }
    break;
  case EA_IDPX:								// indirect (d8, Xn.s, PC)
    ea->addr.l = Chipset.Cpu.PC;
    status = ReadMEM((BYTE *) & ext.w, Chipset.Cpu.PC, 2);
    Chipset.dcycles += 4;
    if (status == BUS_OK) {
      Chipset.Cpu.PC += 2;
      offset.b[0] = ext.d8;
      offset.b[1] = (ext.d8 & 0x80) ? 0xFF : 0x00;			// sign extension
      offset.w[1] = (ext.d8 & 0x80) ? 0xFFFF : 0x0000;			// sign extension
      ea->addr.l += offset.l;
      if (ext.da) {							// index is An
	if (ext.wl) {							// An.L
	  if ((ext.reg == 7) && Chipset.Cpu.SR.S) offset.l = Chipset.Cpu.A[8].l;
	  else offset.l = Chipset.Cpu.A[ext.reg].l;
	} else {							// An.W
	  if ((ext.reg == 7) && Chipset.Cpu.SR.S) offset.w[0] = Chipset.Cpu.A[8].w[0];
	  else offset.w[0] = Chipset.Cpu.A[ext.reg].w[0];
	  offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;	// sign extension
	}
      } else {								// index is Rn
	if (ext.wl) {							// Dn.L
	  offset.l = Chipset.Cpu.D[ext.reg].l;
	} else {							// Dn.W
	  offset.w[0] = Chipset.Cpu.D[ext.reg].w[0];
	  offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;	// sign extension
	}
      }
      ea->addr.l += offset.l;
      Chipset.dcycles += 2;
    }
    break;
  case EA_IMM:
    ea->addr.l = Chipset.Cpu.PC;
    Chipset.Cpu.PC += (ea->isize == 4) ? 4 : 2;
    break;
  case EA_CCR:
  case EA_SR:
  case EA_USP:
    break;
  case EA_BCC:
    ea->addr.l = Chipset.Cpu.PC;
    offset.b[0] = ea->data.b[0];
    if (offset.b[0] == 0x00) {
      status = ReadMEM((BYTE *) & offset.w[0], Chipset.Cpu.PC, 2);
      // Chipset.dcycles += 4; // not usefull here
      if (status != BUS_OK) return status; 
      Chipset.Cpu.PC += 2;
    } else offset.b[1] = (offset.b[0] & 0x80) ? 0xFF : 0x00;
    offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;
    ea->addr.l += offset.l;
    break;
  case EA_DBCC:
    ea->addr.l = Chipset.Cpu.PC;
    status = ReadMEM((BYTE *) & offset.w[0], Chipset.Cpu.PC, 2);
    Chipset.dcycles += 4;
    if (status != BUS_OK) return status;
    Chipset.Cpu.PC += 2;
    offset.w[1] = (offset.w[0] & 0x8000) ? 0xFFFF : 0x0000;
    ea->addr.l += offset.l;
    break;
  default: break;
  }
  return status;
}
//
// Write result at ea.addr or direct with inc ((-An),(An)+) if needed
//
BYTE write_ea(EA *ea, MEM result) {
  BYTE status;

  status = BUS_OK;
  switch (ea->imode) {
  case EA_NONE:
    break;
  case EA_CST:
    emuInfoMessage("EA_CST destination ??");
    break;
  case EA_D:											// direct Dn
    if (ea->isize == 1)Chipset.Cpu.D[ea->reg].b[0] = result.b[0];
    else if (ea->isize == 2) Chipset.Cpu.D[ea->reg].w[0] = result.w[0];
    else if (ea->isize == 4) Chipset.Cpu.D[ea->reg].l = result.l;
    else emuInfoMessage("EA_D isize ??");
    break;
  case EA_A:									// direct An
    if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
      if (ea->isize == 2) {
	result.w[1] = (result.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	Chipset.Cpu.A[8].l = result.l;
      } else if (ea->isize == 4) Chipset.Cpu.A[8].l = result.l;
      else emuInfoMessage("EA_A isize ??");
    } else {
      if (ea->isize == 2) {
	result.w[1] = (result.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	Chipset.Cpu.A[ea->reg].l = result.l;
      } else if (ea->isize == 4) Chipset.Cpu.A[ea->reg].l = result.l;
      else emuInfoMessage("EA_A isize ??");
    }
    break;
  case EA_IAI:									// indirect (An)+
    if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
      Chipset.Cpu.A[8].l += (ea->isize == 4) ? 4 : 2;									// force word align on SP
    } else {
      if (ea->reg == 7) Chipset.Cpu.A[7].l += (ea->isize == 4) ? 4 : 2;		// force word align on SP
      else Chipset.Cpu.A[ea->reg].l += ea->isize;
    }
    status = WriteMEM((BYTE *) (& result), ea->addr.l, ea->isize);
    Chipset.dcycles += (ea->isize == 4) ? 8 : 4;
    break;
  case EA_IAD:									// indirect -(An)
    if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
      Chipset.Cpu.A[8].l = ea->addr.l;						// force word align on SP
    } else {
      Chipset.Cpu.A[ea->reg].l = ea->addr.l;
    }
    status = WriteMEM((BYTE *) (& result), ea->addr.l, ea->isize);
    Chipset.dcycles += (ea->isize == 4) ? 8 : 4;
    break;
  case EA_IA:									// indirect (An)
  case EA_IDA:									// indirect (d16,An)
  case EA_IDAX:									// indirect (d8, Xn.s, An)
  case EA_IXW:
  case EA_IXL:
  case EA_IDP:									// indirect (d16,PC)
  case EA_IDPX:									// indirect (d8, Xn.s, PC)
    status = WriteMEM((BYTE *) (& result), ea->addr.l, ea->isize);
    Chipset.dcycles += (ea->isize == 4) ? 8 : 4;
    break;
  case EA_IMM:
    emuInfoMessage("EA_IMM destination ??");
    break;
  case EA_CCR:
    Chipset.Cpu.SR.r[0] = result.b[0] & 0x1F;		// mask for ccr bits
    break;
  case EA_SR:
    Chipset.Cpu.SR.sr = result.w[0] & 0xA71F;		// mask for sr bits
    break;
  case EA_USP:
    Chipset.Cpu.A[7].l = result.l;
    break;
  default: break;
  }
  return status;
}

//
// read ea.data at ea.addr or direct with inc ((-An),(An)+) if asked (inc)
//
BYTE read_ea(EA *ea, BOOL inc) {
  BYTE status;

  status = BUS_OK;
  switch (ea->imode) {
  case EA_NONE:
    break;
  case EA_CST:
    break;
  case EA_D:									// direct Dn
    if (ea->isize == 1) ea->data.b[0] = Chipset.Cpu.D[ea->reg].b[0];
    else if (ea->isize == 2) ea->data.w[0] = Chipset.Cpu.D[ea->reg].w[0];
    else if (ea->isize == 4) ea->data.l = Chipset.Cpu.D[ea->reg].l;
    else emuInfoMessage("EA_D isize ??");
    break;
  case EA_A:									// direct An
    if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
      if (ea->isize == 2) {
	ea->data.w[0] = Chipset.Cpu.A[8].w[0];
	ea->data.w[1] = (ea->data.w[0] & 0x8000) ? 0xFFFF : 0x0000;		// sign extension
      } else if (ea->isize == 4) ea->data.l = Chipset.Cpu.A[8].l;
      else emuInfoMessage("EA_A isize 1 ??");
    } else {
      if (ea->isize == 2) {
	ea->data.w[0] = Chipset.Cpu.A[ea->reg].w[0];
	ea->data.w[1] = (ea->data.w[0] & 0x8000) ? 0xFFFF : 0x0000;		// sign extension
      } else if (ea->isize == 4) ea->data.l = Chipset.Cpu.A[ea->reg].l;
      else emuInfoMessage("EA_A isize 1 ??");
    }
    break;
  case EA_IAI:									// indirect (An)+
    if (inc) {
      if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
	Chipset.Cpu.A[8].l += (ea->isize == 4) ? 4 : 2;				// force word align on SP
      } else {
	if (ea->reg == 7) Chipset.Cpu.A[7].l += (ea->isize == 4) ? 4 : 2;	// force word align on SP
	else Chipset.Cpu.A[ea->reg].l += ea->isize;
      }
    }
    status = ReadMEM((BYTE *) & ea->data, ea->addr.l, ea->isize);
    Chipset.dcycles += (ea->isize == 4) ? 8 : 4;
    break;
  case EA_IAD:									// indirect -(An)
    if (inc) {
      if (Chipset.Cpu.SR.S && (ea->reg == 7)) {
	Chipset.Cpu.A[8].l = ea->addr.l;
      } else {
	Chipset.Cpu.A[ea->reg].l = ea->addr.l;
      }
    }
    status = ReadMEM((BYTE *) & ea->data, ea->addr.l, ea->isize);
    Chipset.dcycles += (ea->isize == 4) ? 8 : 4;
    break;
  case EA_IA:									// indirect (An)
  case EA_IDA:									// indirect (d16,An)
  case EA_IDAX:									// indirect (d8, Xn.s, An)
  case EA_IXW:									// indirect (xxxx).W
  case EA_IXL:									// indirect (xxxxxxxx).L
  case EA_IDP:									// indirect (d16,PC)
  case EA_IDPX:									// indirect (d8, Xn.s, PC)
    status = ReadMEM((BYTE *) & ea->data, ea->addr.l, ea->isize);
    Chipset.dcycles += (ea->isize == 4) ? 8 : 4;
    break;
  case EA_IMM:
    if (ea->isize == 4) {
      status = ReadMEM((BYTE *) & ea->data.l, ea->addr.l, 4);
      Chipset.dcycles += 8;
    } else {
      status = ReadMEM((BYTE *) & ea->data.w[0], ea->addr.l, 2);
      Chipset.dcycles += 4;
    }
    break;
  case EA_CCR:
    ea->data.b[0] = Chipset.Cpu.SR.r[0];
    Chipset.dcycles += 6;
    break;
  case EA_SR:
    ea->data.w[0] = Chipset.Cpu.SR.sr;
    Chipset.dcycles += 6;
    break;
  case EA_USP:
    ea->data.l = Chipset.Cpu.A[7].l;
    break;
  default: break;
  }
  return status;
}

//################
//#
//#    Public functions
//#
//################

//
// opcode dispatcher
//
VOID EvalOpcode(BYTE newI210) {
  BYTE status;
  OP ope;
  WORD sr_copy;
  WORD traceP;
  //  fprintf(stderr,"%06lX: CPU ", Chipset.Cpu.PC);
  if ((Chipset.Cpu.I210 < 7) & (newI210 == 7)) {	// got a level 7 interrupt
#if defined DEBUG_FETCH
    k = wsprintf(buffer,_T("%06X: CPU : IPL7\n"), Chipset.Cpu.PC);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    Chipset.Cpu.I210 = newI210;
    ope.vector = 24+7;
    goto exception_vect;
  }
  if (newI210 > Chipset.Cpu.SR.MASK) {		 	// got an interrupt
#if defined DEBUG_FETCH
    k = wsprintf(buffer,_T("%06X: CPU : IPL%d\n"), Chipset.Cpu.PC, newI210);
    OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    Chipset.Cpu.I210 = newI210;
    ope.vector = 24+newI210;
    goto exception_vect;
  } 
	
  Chipset.Cpu.I210 = newI210;
	
  if (Chipset.Cpu.State == HALT) {
    Chipset.dcycles += 2;							// elapse some cycles
    return;									// halt state
  }

  traceP = Chipset.Cpu.SR.T;							// save trace bit
  Chipset.Cpu.State = NORMAL;							// normal cpu state
  status = ReadMEM((BYTE *) &byLastOp, Chipset.Cpu.PC, 2);			// read opcode
  Chipset.dcycles += 4;								// 4 cycle more
  if (status != BUS_OK) goto exception_bus;

  //	decode_op(byLastOp, &ope);
  // get the whole decoded struct for the current opcode
  memcopy((BYTE *) (&ope), (BYTE *) (&opes[byLastOp]), sizeof(OP));

  if ((ope.vector == 8) && Chipset.Cpu.SR.S) ope.vector = 0;		// take care of supervisor violations

  if (ope.vector != 0) goto exception_vect;				// jump to exception code

  Chipset.Cpu.PC += 2;							// should be done after exception jump
	
  if (ope.eas.imode != EA_NONE) {					// some EA
    if (ope.sd) {							// monadic op
      status = calc_ea(& ope.eas);					// calc source effective address 
      if (status != BUS_OK) goto exception_bus; 
      if (ope.reas) {							// should read at eas ?
	status = read_ea(& ope.eas, FALSE);				// read aes with no inc on -(An) / (An)+
	if (status != BUS_OK) goto exception_bus;
      }
      status = ((BYTE (*)(OP *)) ope.op)(& ope);			// do the op
      if (status != BUS_OK) goto exception_bus;
      if (ope.vector != 0) goto exception_vect;
      status = write_ea(& ope.eas, ope.result);				// write to eas and do inc
      if (status != BUS_OK) goto exception_bus; 
    } else {								// diadic or unary op
      if (ope.eas.imode != EA_NONE) {
	status = calc_ea(& ope.eas);					// calc source effective address
	if (status != BUS_OK) goto exception_bus; 
	if (ope.reas) {							// should we read eas ?
	  status = read_ea(& ope.eas, TRUE);				// read eas with inc on -(An) / (An)+
	  if (status != BUS_OK) goto exception_bus;
	}
      }
      if (ope.ead.imode != EA_NONE) {					// some ead ?
	status = calc_ea(& ope.ead);					// calc destination effective address
	if (status != BUS_OK) goto exception_bus;
	if (ope.read) {							// should we read it ?
	  status = read_ea(& ope.ead, !(ope.wead));	// read ead with no inc on -(An) / (An)+ if needed
	  if (status != BUS_OK) goto exception_bus;
	}
      }
      status = ((BYTE (*)(OP *)) ope.op)(& ope); 			// do the op
      if (status != BUS_OK) goto exception_bus;
      if (ope.vector != 0) goto exception_vect;
      if ((ope.ead.imode != EA_NONE) && (ope.wead)) {			// should we write the result at ead ? 
	status = write_ea(& ope.ead, ope.result);			// write to ead with inc
	if (status != BUS_OK) goto exception_bus;
      }
    }
  } else {							        // no param op
    status = ((BYTE (*)(OP *)) ope.op)(& ope);				// do the op

    if (status != BUS_OK) goto exception_bus;
    if (ope.vector != 0) goto exception_vect;
  }
  if (Chipset.Cpu.SR.T & traceP) {					// still trace mode valid ?
    ope.vector = 9;							// trace exception
    goto exception_vect;
  }

  return;								// op done, leave

 exception_bus:								// address and bus error
  switch (status) {
  case BUS_ERROR: ope.vector = 2;
    break;
  case ADDRESS_ERROR: ope.vector = 3;
    break;
  }
 exception_vect:							// exception treatment
  Chipset.Cpu.State = EXCEPTION;
  Chipset.Cpu.lastVector = ope.vector;
  sr_copy = Chipset.Cpu.SR.sr;
#ifdef M68000								// for MC68000 
  if (ope.vector > 3) {	// group 1 and 2 exceptions stacks
    Chipset.Cpu.SR.S = 1;
    Chipset.Cpu.SR.T = 0;
    if ((ope.vector > 24) && (ope.vector < 32))	{ // interrupt autovector
      Chipset.Cpu.SR.MASK = ope.vector - 24;
#ifdef DEBUG_FETCH
      k = wsprintf(buffer,_T("%06X: CPU : IPL%d, vector 0x%02X\n"), Chipset.Cpu.PC, newI210, ope.vector);
      OutputDebugString(buffer); buffer[0] = 0x00;
#endif
    }
    DEC4SP;
    status = PUSHL(Chipset.Cpu.PC);					// stack PC
    Chipset.dcycles += 8;
    if (status != BUS_OK) goto exception_bus;
    DEC2SP;
    status = PUSHW(sr_copy);						// stack SR (copy)
    Chipset.dcycles += 4;
    if (status != BUS_OK) goto exception_bus;
    status = ReadMEM((BYTE *)(&Chipset.Cpu.PC), (DWORD) (ope.vector) << 2, 4);
    Chipset.dcycles += 8;
    if (status != BUS_OK) goto exception_bus;
    return;
  } else { // group 0 (RESET, BUS_ERROR, ADDRESS_ERROR) exceptions stacks
    Chipset.Cpu.SR.S = 1;
    Chipset.Cpu.SR.T = 0;
    if (ope.vector == 0) Chipset.Cpu.SR.MASK = 7;		// never append
    DEC4SP;
    status = PUSHL(Chipset.Cpu.PC);				// stack PC
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR	(double bus error)
      return;
    }
    DEC2SP;
    status = PUSHW(sr_copy);					// stack SR
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC2SP;
    status = PUSHW(byLastOp);					// stack instruction
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(Chipset.Cpu.lastMEM);			// stack access address
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC2SP;
    status = PUSHW(Chipset.Cpu.lastRW);				// stack info
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    status = ReadMEM((BYTE *)(&Chipset.Cpu.PC), (DWORD) (ope.vector) << 2, 4);
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
  }
#else 	// for MC68010 
  if (ope.vector > 3) { // group 1 and 2 exceptions stacks
    Chipset.Cpu.SR.S = 1;
    Chipset.Cpu.SR.T = 0;
    if ((ope.vector > 24) & (ope.vector < 32))	{	// interrupt autovector
      Chipset.Cpu.SR.MASK = ope.vector - 24;
    }
    format = 0x0FFF & (ope.vector << 2);			// format 0000
    DEC2SP;
    status = PUSHW(format);					// stack format & offset	1 word
    Chipset.dcycles += 4;
    if (status != BUS_OK) goto exception_bus;
    DEC4SP;
    status = PUSHL(Chipset.Cpu.PC);				// stack PC     		3 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) goto exception_bus;
    DEC2SP;
    status = PUSHW(sr_copy);					// stack SR			4 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) goto exception_bus;
    status = ReadMEM((BYTE *)(&Chipset.Cpu.PC), (DWORD) (ope.vector) << 2, 4);
    Chipset.dcycles += 8;
    if (status != BUS_OK) goto exception_bus;
    return;
  } else { // group 0 (RESET, BUS_ERROR, ADDRESS_ERROR) exceptions stacks
    Chipset.Cpu.SR.S = 1;
    Chipset.Cpu.SR.T = 0;
    if (ope.vector == 0) Chipset.Cpu.SR.MASK = 7;		// never append

    info = 0xDEADF00D;
    DEC4SP;
    status = PUSHL(info);					// stack info			2 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(info);					// stack info			4 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(info);					// stack info			6 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(info);					// stack info			8 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(info);					// stack info			10 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(info);					// stack info			12 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(info);					// stack info			14 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    info = 0x1C00;						// version in bits 10-13	
    DEC4SP;
    status = PUSHL(info);					// stack version number		16 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC2SP;
    status = PUSHW(byLastOp);					// stack instruction		17 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    data = 0xFFFF;
    DEC2SP;
    status = PUSHW(data);					// stack reserved		18 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    data = ope.eas.data.w[0];
    DEC2SP;
    status = PUSHW(data);					// stack input buffer		19 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    data = 0xFFFF;
    DEC2SP;
    status = PUSHW(data);					// stack reserved		20 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    data = ope.result.w[0];
    DEC2SP;
    status = PUSHW(data);					// stack output buffer		21 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    data = 0xFFFF;
    DEC2SP;
    status = PUSHW(data);					// stack reserved		22 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(Chipset.Cpu.lastMEM);			// stack fault address		24 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    data = (Chipset.Cpu.lastRW) ? 0x0100 : 0x0000;
    DEC2SP;
    status = PUSHW(data);					// stack status word		25 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    format = 0x8000 | (0x0FFF & (ope.vector << 2));	        // format 1000
    DEC2SP;
    status = PUSHW(format);					// stack format & offset	26 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC4SP;
    status = PUSHL(Chipset.Cpu.PC);				// stack PC			28 words
    Chipset.dcycles += 8;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    DEC2SP;
    status = PUSHW(sr_copy);					// stack SR			29 words
    Chipset.dcycles += 4;
    if (status != BUS_OK) {
      Chipset.Cpu.State = HALT;					// another ERROR
      return;
    }
    status = ReadMEM((BYTE *)(&Chipset.Cpu.PC), (DWORD) (ope.vector) << 2, 4);
    Chipset.dcycles += 8;
    if (status != BUS_OK) goto exception_bus;
    return;
  }
#endif
  return;
}

//
// initialize the opcodes table
//
VOID initOP(VOID) {
  DWORD i;

  for (i = 0; i < 65536; i++) {					// loop on all 16 bits opcodes
    memzero((BYTE *) (&opes[i]), sizeof(OP));			// zeroes it
    decode_op((WORD) i, opes+i);				// save current op table
  }
}
