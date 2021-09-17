/*
 *   Opcodes.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   Real code for decoding opcodes :)
//

#include "common.h"
#include "hp9816emu.h"
// #include "opcodes.h"
#include "mops.h"

// #define w Chipset.CPU

#include "ops.h"

//
// decode an op-code
//
VOID decode_op(WORD op, OP *ope) {
  BYTE op4_3 = (op & 0x0018) >> 3;
  BOOL op5 = (op & 0x0020) ? 1 : 0;
  BOOL op6 = (op & 0x0040) ? 1 : 0;
  BOOL op7 = (op & 0x0080) ? 1 : 0;
  BOOL op8 = (op & 0x0100) ? 1 : 0;
  BOOL op11 = (op & 0x0800) ? 1 : 0;
  BYTE op11_8 = (op & 0x0F00) >> 8;
  BYTE op11_9 = (op & 007000) >> 9;
  BOOL op14 = (op & 0x4000) ? 1 : 0;
  
  BYTE ea_reg = (op & 000007);
  BYTE ea_mode = (op & 000070) >> 3;
  BYTE ea_size = (op & 000300) >> 6;
  BOOL ea_size11 = (ea_size == 0x3);
  
  BOOL ea_d = (ea_mode == 0);
  BOOL ea_a = (ea_mode == 1);
  BOOL ea_ia = (ea_mode == 2);
  BOOL ea_iai = (ea_mode == 3);
  BOOL ea_iad = (ea_mode == 4);
  BOOL ea_ida = (ea_mode == 5);
  BOOL ea_idax = (ea_mode == 6);
  BOOL ea_ixw = (ea_mode == 7) && (ea_reg == 0);
  BOOL ea_ixl = (ea_mode == 7) && (ea_reg == 1);
  BOOL ea_idp = (ea_mode == 7) && (ea_reg == 2);
  BOOL ea_idpx = (ea_mode == 7) && (ea_reg == 3);
  BOOL ea_imm = (ea_mode == 7) && (ea_reg == 4);
  
  BYTE ea_imode = (ea_d) ? EA_D : (ea_a) ? EA_A : (ea_ia) ? EA_IA : (ea_iai) ? EA_IAI :
    (ea_iad) ? EA_IAD : (ea_ida) ? EA_IDA : (ea_idax) ? EA_IDAX : (ea_ixw) ? EA_IXW :
    (ea_ixl) ? EA_IXL : (ea_idp) ? EA_IDP : (ea_idpx) ? EA_IDPX : (ea_imm) ? EA_IMM : EA_NONE;
  
  BOOL ea_valid = (ea_imode != EA_NONE);
  BOOL ea_data = ea_valid && !(ea_a);
  BOOL ea_memory = ea_valid && !(ea_d || ea_a);
  BOOL ea_control = ea_valid && !(ea_d || ea_a || ea_iai || ea_iad || ea_imm);
  BOOL ea_alterable = ea_valid && !(ea_idp || ea_idpx || ea_imm);
  
  BYTE ea2_reg = (op & 007000) >> 9;
  BYTE ea2_mode = (op & 00700) >> 6;
  BOOL ea2_d = (ea2_mode == 0);
  BOOL ea2_a = (ea2_mode == 1);
  BOOL ea2_ia = (ea2_mode == 2);
  BOOL ea2_iai = (ea2_mode == 3);
  BOOL ea2_iad = (ea2_mode == 4);
  BOOL ea2_ida = (ea2_mode == 5);
  BOOL ea2_idax = (ea2_mode == 6);
  BOOL ea2_ixw = (ea2_mode == 7) && (ea2_reg == 0);
  BOOL ea2_ixl = (ea2_mode == 7) && (ea2_reg == 1);
  BOOL ea2_idp = (ea2_mode == 7) && (ea2_reg == 2);
  BOOL ea2_idpx = (ea2_mode == 7) && (ea2_reg == 3);
  BOOL ea2_imm = (ea2_mode == 7) && (ea2_reg == 4);
  
  BYTE ea2_imode = (ea2_d) ? EA_D : (ea2_a) ? EA_A : (ea2_ia) ? EA_IA : (ea2_iai) ? EA_IAI :
    (ea2_iad) ? EA_IAD : (ea2_ida) ? EA_IDA : (ea2_idax) ? EA_IDAX : (ea2_ixw) ? EA_IXW :
    (ea2_ixl) ? EA_IXL : (ea2_idp) ? EA_IDP : (ea2_idpx) ? EA_IDPX : (ea2_imm) ? EA_IMM : EA_NONE;
  
  BOOL ea2_valid = (ea2_imode != EA_NONE);
  BOOL ea2_data = ea2_valid && !(ea2_a);
  BOOL ea2_memory = ea2_valid && !(ea2_d || ea2_a);
  BOOL ea2_control = ea2_valid && !(ea2_d || ea2_a || ea2_iai || ea2_iad || ea2_imm);
  BOOL ea2_alterable = ea2_valid && !(ea2_idp || ea2_idpx || ea2_imm);
  
  BOOL ill_op = FALSE;
	
  EA ea1, ea2;
  
  ea1.isize = (ea_size == 0) ? 1 : (ea_size == 1) ? 2 : 4;
  ea1.imode = ea_imode;
  ea1.reg = ea_reg;
  ea1.data.l = 0x00000000;
  
  ea2.isize = ea1.isize;
  ea2.imode = ea2_imode;
  ea2.reg = ea2_reg;
  ea2.data.l = 0x00000000;
  
  ope->eas.imode = EA_NONE;	        // no eas
  ope->eas.isize = 0;			// no isize
  ope->ead.imode = EA_NONE;	        // no ead
  ope->ead.isize = 0;			// no isize
  ope->vector = 0;			// no exception
  ope->eas.data.l = 0x00000000;
  ope->ead.data.l = 0x00000000;
  ope->sd = FALSE;
  ope->reas = TRUE;			// read eas before op
  ope->read = TRUE;			// read ead before op
  ope->wead = TRUE;			// write ead after op
  
  switch ((op & 0xF000) >> 12) {
  case 0x0:	// IMM and other
    if (op8) {								// other ?
      if (ea_a) {							// MOVEP ?
	if (op7) {							// MOVEP Dx, (d,Ay)
	  ope->eas.imode = EA_D; ope->eas.reg = ea2.reg;
	  ope->ead.imode = EA_IDA; ope->ead.reg = ea1.reg;
	  ope->eas.isize = (op6) ? 4 : 2;
	  ope->read = FALSE;						// no read ead before op
	  ope->wead = FALSE;						// no write after op either (done in the op)
	  ope->op = OP_movep_1;
	} else {							// MOVEP (d,Ay),Dx
	  ope->eas.imode = EA_IDA; ope->eas.reg = ea1.reg;
	  ope->eas.isize = (op6) ? 4 : 2;
	  ope->ead.imode = EA_D; ope->ead.reg = ea2.reg;
	  ope->ead.isize = (op6) ? 4 : 2;				// need it from write_ea
	  ope->reas = FALSE;						// no read, will be done by the op_movep2
	  ope->read = FALSE;						// no read ead before op
	  ope->op = OP_movep_2;
	}
      } else if (ea_size == 0) {					// BTST Dn,Ea ?
	if (ea_data && !ea_imm) {					// BTST Dn,Ea
	  ope->eas = ea2; ope->eas.imode = EA_D;
	  ope->eas.isize = 1;								
	  ope->ead = ea1;
	  ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
	  ope->op = OP_btst;
	  ope->wead = FALSE;			// no write ead after for btst
	} else ill_op = TRUE;
      } else if (ea_data && ea_alterable) {			// BCHG BCLR BSET Dn,Ea
	ope->eas = ea2; ope->eas.imode = EA_D;
	ope->eas.isize = 1;								
	ope->ead = ea1;
	ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
	ope->op = (ea_size == 1) ? OP_bchg : (ea_size == 2) ? OP_bclr : OP_bset;
      } else ill_op = TRUE;
    }
    else {								// imm or other
      ope->eas.imode = EA_IMM;						// imm
      ope->eas.isize = ea1.isize;
      ope->ead = ea1;
      switch (op11_9) {
      case 0x0: 							// ORI
      case 0x1:								// ANDI
      case 0x5:								// EORI
	ope->op = (op11_9 == 0) ? OP_or : (op11_9 == 1) ? OP_and : OP_eor;
	if (ea_imm) {
	  if (ea_size == 0) {					        // #,CCR
	    ope->ead.imode = EA_CCR;		
	    ope->op = (op11_9 == 0) ? OP_oricr : (op11_9 == 1) ? OP_andicr : OP_eoricr;
	  } else if (ea_size == 1) {			                // #,SR
	    // if (Chipset.Cpu.SR.S) {
	    ope->ead.imode = EA_SR;
	    ope->op = (op11_9 == 0) ? OP_orisr : (op11_9 == 1) ? OP_andisr : OP_eorisr;
	    //} else 
	    ope->vector = 8;
	  } else ill_op = TRUE;
	} else if (!ea_data || !ea_alterable) ill_op = TRUE;
	break;
      case 0x2:								// SUBI
      case 0x3:								// ADDI
      case 0x6:								// CMPI
	if (ea_size11) ill_op = TRUE;
	else if (ea_data && ea_alterable) {
	  ope->op = (op11_9 == 2) ? OP_sub : (op11_9 == 3) ? OP_add : OP_cmp;
	  if (op11_9 == 6) ope->wead = FALSE;				// no write ead after op
	} else ill_op = TRUE;
	break;
      case 0x4:								// BTST BCHG BCLR BSET #,Ea ?
	if (ea_size == 0) {						// BTST #,Ea ?
	  if (ea_data && !ea_imm) {				        // BTST #,Ea
	    ope->op = OP_btst;
	    ope->eas.isize = 1;
	    ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
	    ope->wead = FALSE;	// no write ead after btst
	  } else ill_op = TRUE;
	} else if (ea_data && ea_alterable) {// BCHG BCLR BSET #,Ea ?
	  ope->op = (ea_size == 1) ? OP_bchg : (ea_size == 2) ? OP_bclr : OP_bset;
	  ope->eas.isize = 1;
	  ope->ead.isize = (ope->ead.imode == EA_D) ? 4 : 1;
	} else ill_op = TRUE;
	break;
#ifdef M68000
      default: ill_op = TRUE;
#else
      case 0x07:						        // MOVES for 68010 and up
	if (ea_memory && ea_alterable) {
	  //if (Chipset.Cpu.SR.S) 
	  ope->op = OP_moves;
	  //else 
	  ope->vector = 8;
	} else ill_op = TRUE;
	break;
#endif
      }
    }
    break;
  case 0x1:	// MOVE.B
    if (ea_valid && ea2_data && ea2_alterable && !ea_a) {
      ope->eas = ea1; ope->ead = ea2;
      ope->eas.isize = 1;
      ope->ead.isize = 1;
      ope->read = FALSE;					// no read ead before op
      ope->op = OP_moveb;
    } else ill_op = TRUE;
    break;
  case 0x2:	// MOVE.L MOVEA.L
    if (ea_valid && ((ea2_data && ea2_alterable) || ea2_a)) {
      ope->eas = ea1; ope->ead = ea2;
      ope->eas.isize = 4;
      ope->ead.isize = 4;
      ope->read = FALSE;					// no read ead before op
      ope->op = (ea2_a) ? OP_moveal: OP_movel;
    } else ill_op = TRUE;
    break;
  case 0x3:	// MOVE.W MOVEA.W
    if (ea_valid && ((ea2_data && ea2_alterable) || ea2_a)) {
      ope->eas = ea1; ope->ead = ea2;
      ope->eas.isize = 2;
      ope->ead.isize = (ea2_a) ? 4 : 2;
      ope->read = FALSE;					// no read ead before op
      ope->op = (ea2_a) ? OP_moveaw : OP_movew;
    } else ill_op = TRUE;
    break;	
  case 0x4:	// MISC
    if (op8) {							// CHK LEA ?
      if (op6) {						// LEA ?
	if (op7) {						// LEA ?
	  if (ea_control) {					// LEA Ea,An
	    ope->eas = ea1; ope->ead = ea2;    
	    ope->eas.isize = 4;					// usefull ?
	    ope->ead.imode = EA_A;
	    ope->ead.isize = 4;
	    ope->reas = FALSE;					// only calc eas
	    ope->read = FALSE;					// no read ead before op
	    ope->op = OP_lea;
	  } else ill_op = TRUE;
	} else ill_op = TRUE;
      } else if (op7) {
	if (ea_data) {						// CHK Ea,Dn
	  ope->eas = ea1; ope->eas.isize = 2;
	  ope->ead = ea2; ope->ead.isize = 2;
	  ope->ead.imode = EA_D;
	  ope->wead = FALSE;					// no write after op
	  ope->op = OP_chk;
	} else ill_op = TRUE;
      } else ill_op = TRUE;
    }
    else {							// MISC ?
      switch (op11_9) {
      case 0x0:							// NEGX MOVE SR ?
	if (ea_data && ea_alterable) {
	  if (ea_size11) {					// MOVE SR,Ea
#ifdef M68000
#else
	    //if (Chipset.Cpu.SR.S) {				// privileged for 68010 and up
#endif
	    ope->eas.imode = EA_SR;
	    ope->eas.isize = 2;				
	    ope->ead = ea1;
	    ope->ead.isize = 2;
	    ope->read = FALSE;					// no read ead before op
	    ope->op = OP_movesr;
#ifdef M68000
#else
	    //} else 
	    ope->vector = 8;
#endif
	  } else {						// NEGX Ea
	    ope->eas = ea1;
	    ope->sd = TRUE;
	    ope->op = OP_negx;
	  }
	} else ill_op = TRUE;
	break;
      case 0x1:							// CLR MOVE CCR ?
	if (ea_data && ea_alterable) {
	  if (ea_size11) {					// MOVE CCR,Ea
#ifdef M68000
	    ill_op = TRUE;					// illegal for 68000
#else								// for 68010 and up
	    ope->eas.imode = EA_CCR;
	    ope->eas.isize = 1;
	    ope->ead = ea1;
	    ope->ead.isize = 1;
	    ope->read = FALSE;					// no read ead before op
	    ope->op = OP_moveccr;	
#endif
	  } else {						// CLR Ea
	    ope->eas = ea1;
	    // ope->read = FALSE;				// we will clear it anyway :)
	    ope->sd = TRUE;
	    ope->op = OP_clr;		
	  }
	} else ill_op = TRUE;
	break;
      case 0x2:							// NEG MOVE CCR ?
	if (ea_size11) {					// MOVE Ea,CCR ?
	  if (ea_data) {					// MOVE Ea,CCR
	    ope->eas = ea1;
	    ope->eas.isize = 2;					// word size !
	    ope->ead.imode = EA_CCR;
	    ope->read = FALSE;					// no read ead before op
	    ope->op = OP_moveccr;
	  } else ill_op = TRUE;
	} else if (ea_data && ea_alterable) {	           	// NEG
	  ope->eas = ea1;
	  ope->sd = TRUE;
	  ope->op = OP_neg;
	} else ill_op = TRUE;
	break;
      case 0x3:							// NOT MOVE SR ?
	if (ea_size11) {					// MOVE Ea,SR ?
	  if (ea_data) {					// MOVE Ea,SR
	    //if (Chipset.Cpu.SR.S) {
	    ope->eas = ea1;
	    ope->eas.isize = 2;
	    ope->ead.imode = EA_SR;
	    ope->read = FALSE;					// no read ead before op
	    ope->op = OP_movesr;
	    //} else 
	    ope->vector = 8;
	  } else ill_op = TRUE;
	} else if (ea_data && ea_alterable) {		        // NOT
	  ope->eas = ea1;
	  ope->sd = TRUE;
	  ope->op = OP_not;
	} else ill_op = TRUE;
	break;
      case 0x4:							// NBCD SWAP BKP PEA EXT MOVEM ?
	switch (ea_size) {		
	case 0: 
	  if (ea_data && ea_alterable) {		        // NBCD Ea
	    ope->eas = ea1;
	    ope->eas.isize = 1;
	    ope->sd = TRUE;
	    ope->op = OP_nbcd;
	  } else ill_op = TRUE;
	  break;
	case 1:
	  if (ea_d) {						// SWAP Dn
	    ope->eas = ea1;
	    ope->eas.isize = 4;
	    ope->sd = TRUE;
	    ope->op = OP_swap;
	  } else if (ea_a) {					// BKP (68010)
	    ill_op = TRUE;
	  } else if (ea_control) {			        // PEA Ea
	    ope->eas = ea1;
	    ope->eas.isize = 4;
	    ope->reas = FALSE;					// only calc eas 
	    ope->op = OP_pea;
	  } else ill_op = TRUE;
	  break;
	default:
	  if (ea_d) {						// EXT Dn
	    ope->eas = ea1;
	    ope->eas.isize = (ea_size == 2) ? 2 : 4;
	    ope->sd = TRUE;
	    ope->op = OP_ext;
	  } else if ((ea_control && ea_alterable) || ea_iad) {	// MOVEM L,Ea
	    ope->eas.imode = EA_IMM;				// to get L
	    ope->eas.isize = 2;
	    ope->ead = ea1;
	    ope->ead.isize = (op6) ? 4 : 2;
	    ope->read = FALSE;					// no read ead before op
	    ope->wead = FALSE;					// no write after either
	    ope->op = OP_movemlea;
	  } else ill_op = TRUE;
	  break;
	}
	break;
      case 0x5:							// TST TAS ILLEGAL ?
	if (ea_size11) {
	  if (ea_data && ea_alterable) {			// TAS Ea
	    ope->eas = ea1;
	    ope->eas.isize = 1;
	    ope->sd = TRUE;
	    ope->op = OP_tas;
	  } else ill_op = TRUE;					// ILLEGAL and more
	} else if (ea_data && ea_alterable) {		        // TST Ea
	  ope->eas = ea1;
	  // ope->wead = FALSE;
	  ope->op = OP_tst;
	} else ill_op = TRUE;
	break;
      case 0x6:							// MOVEM ?
	if (op7) {						// MOVEM Ea,L
	  if (ea_control || ea_iai) {
	    ope->eas.imode = EA_IMM;				// to get L
	    ope->eas.isize = 2;
	    ope->ead = ea1;
	    ope->ead.isize = (op6) ? 4 : 2;
	    ope->read = FALSE;					// compute only ead
	    ope->wead = FALSE;					// write back if needed done in op
	    ope->op = OP_movemeal;
	  } else ill_op = TRUE;
	} else ill_op = TRUE;
	break;
      default:							// TRAP LINK UNLK MOVE USP RESET NOP STOP RTE RTD RTS TRAPV RTR JSR JMP MOVEC ?
	if (op7) {						// JSR JMP ?
	  if (op6) {						// JMP ?
	    if (ea_control) {					// JMP Ea
	      ope->eas = ea1;
	      ope->eas.isize = 4;
	      ope->reas = FALSE;
	      ope->op = OP_jmp;
	    } else ill_op = TRUE;
	  } else if (ea_control) {				// JSR Ea
	    ope->eas = ea1;
	    ope->eas.isize = 4;
	    ope->reas = FALSE;
	    ope->op = OP_jsr;
	  } else ill_op = TRUE;
	} else if (op6) {
	  if (ea_a || ea_d) {					// TRAP
	    ope->eas.imode = EA_CST;
	    ope->eas.isize = 1;
	    ope->eas.data.b[0] = (op & 0x000F);
	    ope->reas = FALSE;
	    ope->op = OP_trap;
	  }
	  else if (ea_ia) {					// LINK
	    ope->eas = ea1; ope->eas.imode = EA_A;
	    ope->eas.isize = 4;
	    ope->ead.imode = EA_IMM;
	    ope->ead.isize = 2;
	    ope->wead = FALSE;
	    ope->op = OP_link;
	  }
	  else if (ea_iai) {					// UNLK
	    ope->eas = ea1; ope->eas.imode = EA_A;
	    ope->eas.isize = 4;
	    ope->sd = TRUE;
	    ope->op = OP_unlk;
	  }
	  else if (ea_iad) {					// MOVE An,USP
	    //if (Chipset.Cpu.SR.S) {
	    ope->eas = ea1; ope->eas.imode = EA_A;
	    ope->eas.isize = 4;
	    ope->ead.imode = EA_USP;
	    ope->ead.isize = 4;
	    ope->read = FALSE;					// no read ead before op
	    ope->op = OP_moveal;
	    //} else 
	    ope->vector = 8;
	  }
	  else if (ea_ida) {					// MOVE USP,An
	    //if (Chipset.Cpu.SR.S) {
	    ope->ead = ea1; ope->ead.imode = EA_A;
	    ope->ead.isize = 4;
	    ope->read = FALSE;					// no read ead before op
	    ope->eas.isize = 4;
	    ope->eas.imode = EA_USP;
	    ope->op = OP_moveal;
	    //} else 
	    ope->vector = 8;
	  }
	  else if (ea_idax) {					// MISC ?
	    switch (ea_reg) {
	    case 0:						// RESET
	      //if (Chipset.Cpu.SR.S) 
	      ope->op = OP_reset;
	      //else 
	      ope->vector = 8;
	      break;
	    case 1:						// NOP
	      ope->op = OP_nop;
	      break;
	    case 2:						// STOP
	      ope->eas.imode = EA_IMM;
	      ope->eas.isize = 2;
	      //if (Chipset.Cpu.SR.S) 
	      ope->op = OP_stop;
	      //else 
	      ope->vector = 8;
	      break;
	    case 3:						// RTE
	      //if (Chipset.Cpu.SR.S) 
	      ope->op = OP_rte;
	      //else 
	      ope->vector = 8;
	      break;
	    case 5:						// RTS
	      ope->op = OP_rts;
	      break;
	    case 6:						// TRAPV
	      ope->op = OP_trapv;
	      break;
	    case 7:						// RTR
	      ope->op = OP_rtr;
	      break;
	    default: ill_op = TRUE;				// RTD (68010)
	    }
	  } 
#ifdef M68000
	  else ill_op = TRUE;
#else
	  else if (ea_idp || ea_idpx) {				// MOVEC
	    //if (Chipset.Cpu.SR.S) {
	    ope->op = (ea_idpx) ? OP_movecnc : OP_moveccn;
	    //} else 
	    ope->vector = 8;
	  }
	  else ill_op = TRUE;
#endif
	} else ill_op = TRUE;
      }
    }
    break;
  case 0x5:	                                                // ADDQ SUBQ DBcc Scc 
    if (ea_size11) {						// DBcc Scc ?
      if (ea_a) {						// DBcc Dn,d16
	ope->eas = ea1; ope->eas.imode = EA_D;
	ope->eas.isize = 2;
	ope->ead.imode = EA_DBCC;
	ope->ead.data.b[0] = op11_8;
	ope->read = FALSE;
	ope->wead = FALSE;
	ope->op = OP_dbcc;
      } else if (ea_data && ea_alterable) {			// Scc Ea
	ope->eas = ea1;
	ope->eas.isize = 1;
	ope->eas.data.b[2] = op11_8;				// b[0] scratched by the read ... 
	ope->reas = TRUE;					// for best 68000 compatibility (but not for 68010 and up)
	ope->sd = TRUE;
	ope->op = OP_scc;
      } else ill_op = TRUE;
    }
    else {							// ADDQ SUBQ ?
      if (ea_alterable) {					// ADDQ SUBQ
	ope->eas.imode = EA_CST;
	ope->eas.data.b[0] = (op11_9 == 0) ? 8 : op11_9;	// ope->eas.data.l is 0 at start so no extension problems
	ope->reas = FALSE;
	ope->ead = ea1;
	if (ope->ead.imode == EA_A) {				// no size, always long even with word ;)
	  if (ea_size == 0) ill_op = TRUE;			// but illegal for byte
	  else {
	    ope->ead.isize = 4;
	    ope->eas.isize = 4;
	    ope->op = (op8) ? OP_suba : OP_adda;	        // no condition codes modified
	  }
	}
	else {
	  ope->eas.isize = ope->ead.isize;
	  ope->op = (op8) ? OP_sub : OP_add;
	}
      } else ill_op = TRUE;
    }
    break;
  case 0x6:	                                                // BRA BSR Bcc
    switch (op11_8) {
    case 0:							// BRA d16
      ope->eas.data.b[0] = (op & 0x00FF);
      ope->eas.imode = EA_BCC;
      ope->reas = FALSE;
      ope->op = OP_bra;
      break;
    case 1:							// BSR d16
      ope->eas.data.b[0] = (op & 0x00FF);
      ope->eas.imode = EA_BCC;
      ope->reas = FALSE;
      ope->op = OP_bsr;
      break;
    default:							// Bcc d16
      ope->eas.data.b[1] = op11_8;
      ope->eas.data.b[0] = (op & 0x00FF);
      ope->eas.imode = EA_BCC;
      ope->reas = FALSE;
      ope->op = OP_bcc;
      break;
    }
    break;
  case 0x7:	// MOVEQ
    if (op8) ill_op = TRUE;
    else {							// MOVEQ #,Dn
      ope->eas.imode = EA_CST; 
      ope->eas.data.b[0] = (BYTE) (op & 0x00FF);
      ope->reas = FALSE; 
      ope->ead = ea2; ope->ead.imode = EA_D;
      ope->ead.isize = 4;
      ope->read = FALSE;					// no ead read before op
      ope->op = OP_moveq;
    }
    break;
  case 0x8:	// SBCD OR DIVx
    if (ea_size11) {						// DIVx ?
      if (ea_data) {						// DIVx
	ope->eas = ea1;
	ope->eas.isize = 2;
	ope->ead = ea2; ope->ead.imode = EA_D;
	ope->ead.isize = 4;
	ope->op = (op8) ? OP_divs : OP_divu;
      } else ill_op = TRUE;
    } else if (op8) {						// SBCD OR ?
      if (ea_d) {						// SBCD ?
	if (ea_size == 0) {					// SBCD Dx,Dy
	  ope->eas = ea1; ope->eas.imode = EA_D;
	  ope->eas.isize = 1;
	  ope->ead = ea2; ope->ead.imode = EA_D;
	  ope->ead.isize = 1;
	  ope->op = OP_sbcd;
	} else ill_op = TRUE;
      } else if (ea_a) {					// SBCD ?
	if (ea_size == 0) {					// SBCD -(Ax),-(Ay)
	  ope->eas = ea1; ope->eas.imode = EA_IAD;
	  ope->eas.isize = 1;
	  ope->ead = ea2; ope->ead.imode = EA_IAD;
	  ope->ead.isize = 1;
	  ope->op = OP_sbcd;
	}
      } else if (ea_memory && ea_alterable) {			// OR Dn,Ea
	ope->ead = ea1;
	ope->eas = ea2; ope->eas.imode = EA_D;
	//					ope->eas.isize = ope->ead.isize;
	ope->op = OP_or;
      } else ill_op = TRUE;
    } else if (ea_data) {					// OR Ea,Dn
      ope->eas = ea1;
      ope->ead = ea2; ope->ead.imode = EA_D;
      //				ope->ead.isize = ope->eas.isize;
      ope->op = OP_or;
    } else ill_op = TRUE;
    break;
  case 0x9:	// ADD
  case 0xD:	// SUB
    if (ea_size11) {						// ADDA ?
      if (ea_valid) {						// ADDA Ea,An
	ope->eas = ea1;
	ope->eas.isize = (op8) ? 4 : 2;
	ope->ead = ea2; ope->ead.imode = EA_A;
	ope->ead.isize = 4;
	ope->op = (op14) ? OP_adda : OP_suba;
      } else ill_op = TRUE;
    } else {							// ADD ADDX ?
      if (op8) {						// ADD ADDX ?
	if (ea_a) {						// ADDX -(Ay),-(Ax)
	  ope->eas = ea1; ope->eas.imode = EA_IAD;
	  ope->ead = ea2; ope->ead.imode = EA_IAD;
	  ope->op = (op14) ? OP_addx : OP_subx;
	} else if (ea_d) {					// ADDX Dy,Dx
	  ope->eas = ea1; ope->eas.imode = EA_D;
	  ope->ead = ea2; ope->ead.imode = EA_D;
	  ope->op = (op14) ? OP_addx : OP_subx;
	} else if (ea_memory && ea_alterable) {			// ADD Dn,Ea
	  ope->eas = ea2; ope->eas.imode = EA_D;
	  ope->ead = ea1;
	  ope->op = (op14) ? OP_add : OP_sub;
	} else ill_op = TRUE;
      } else if (ea_valid) {					// ADD Ea,Dn
	if ((ea_a) && (ea_size == 0)) ill_op = TRUE;	        // ADD.B An,Dn forbidden
	else {
	  ope->eas = ea1;
	  ope->ead = ea2; ope->ead.imode = EA_D;
	  ope->op = (op14) ? OP_add : OP_sub;
	}
      } else ill_op = TRUE;
    }
    break;
  case 0xA:	// LINEA
    ope->vector = 0x0A;
    break;
  case 0xB:	// CMP EOR CMPA CMPM
    if (ea_size11) {						// CMPA ?
      if (ea_valid) {						// CMPA Ea,An
	ope->eas = ea1;
	ope->eas.isize = (op8) ? 4 : 2;
	ope->ead = ea2; ope->ead.imode = EA_A;
	ope->ead.isize = 4;
	ope->wead = FALSE;
	ope->op = OP_cmpa;
      } else ill_op = TRUE;
    } else if (op8) {						// EOR CMPM ?
      if (ea_a) {						// CMPM (Ay)+,(Ax)+
	ope->eas = ea1; ope->eas.imode = EA_IAI;
	ope->ead = ea2; ope->ead.imode = EA_IAI;
	ope->wead = FALSE;					// no write ead after op
	ope->op = OP_cmp;
      } else if (ea_data && ea_alterable) {			// EOR Dn,Ea
	ope->eas = ea2; ope->eas.imode = EA_D;
	ope->ead = ea1;
	ope->op = OP_eor;
      } else ill_op = TRUE;
    } else if (ea_valid) {					// CMP Ea,Dn
      if ((ea_a) && (ea_size == 0)) ill_op = TRUE;	        // CMP.B An,Dn forbidden
      else {
	ope->eas = ea1;
	ope->ead = ea2; ope->ead.imode = EA_D;
	ope->wead = FALSE;					// no write ead after op
	ope->op = OP_cmp;
      }
    } else ill_op = TRUE;
    break;
  case 0xC:	// AND MUL ABCD EXG
    if (ea_size11) {						// MULx ?
      if (ea_data) {						// MUL Ea,Dn
	ope->eas = ea1;
	ope->eas.isize = 2;
	ope->ead = ea2; ope->ead.imode = EA_D;
	ope->ead.isize = 4;
	ope->op = (op8) ? OP_muls : OP_mulu;
      } else ill_op = TRUE;
    } else if (op8) {						// AND ABCD EXG ?
      if (ea_a) {						// ABCD EXG ?
	if (ea_size == 0) {					// ABCD -(Ay),-(Ax)
	  ope->eas = ea1; ope->eas.imode = EA_IAD;
	  ope->eas.isize = 1;
	  ope->ead = ea2; ope->ead.imode = EA_IAD;
	  ope->ead.isize = 1;
	  ope->op = OP_abcd;
	} else if (ea_size == 1) {				// EXG Ax,Ay
	  ope->eas = ea1; ope->eas.imode = EA_A;
	  ope->eas.isize = 4;
	  ope->ead = ea2; ope->ead.imode = EA_A;
	  ope->ead.isize = 4;
	  ope->op = OP_exgaa;
	} else {						// EXG Dx,Ay
	  ope->eas = ea1; ope->eas.imode = EA_A;
	  ope->eas.isize = 4;
	  ope->ead = ea2; ope->ead.imode = EA_D;
	  ope->ead.isize = 4;
	  ope->op = OP_exgad;
	}
      } else if (ea_d) {					// ABCD EXG ?
	if (ea_size == 0) {					// ABCD Dy,Dx
	  ope->eas = ea1; ope->eas.imode = EA_D;
	  ope->eas.isize = 1;
	  ope->ead = ea2; ope->ead.imode = EA_D;
	  ope->ead.isize = 1;
	  ope->op = OP_abcd;
	} else if (ea_size == 1) {				// EXG Dy,Dx
	  ope->eas = ea1; ope->eas.imode = EA_D;
	  ope->eas.isize = 4;
	  ope->ead = ea2; ope->ead.imode = EA_D;
	  ope->ead.isize = 4;
	  ope->op = OP_exgdd;
	} else ill_op = TRUE;
      } else if (ea_memory && ea_alterable) {			// AND Dn,Ea
	ope->ead = ea1;
	ope->eas = ea2; ope->eas.imode = EA_D;
	ope->op = OP_and;
      } else ill_op = TRUE;
    } else {							// AND ?
      if (ea_data) {						// AND Ea,Dn
	ope->eas = ea1;
	ope->ead = ea2; ope->ead.imode = EA_D;
	ope->op = OP_and;
      } else ill_op = TRUE;
    }
    break;
  case 0xE:	                                                // ASx LSx ROXs ROx
    if (ea_size11) {						// xxx Ea ?
      if (op11) ill_op = TRUE;
      else if (ea_memory && ea_alterable) {			// xxx Ea
	switch (op11_9) {
	case 0: ope->op = (op8) ? OP_aslm : OP_asrm; break;
	case 1: ope->op = (op8) ? OP_lslm : OP_lsrm; break;
	case 2: ope->op = (op8) ? OP_roxlm : OP_roxrm; break;
	case 3: ope->op = (op8) ? OP_rolm : OP_rorm; break;
	}
	ope->eas = ea1;
	ope->eas.isize = 2;
	ope->sd = TRUE;
      } else ill_op = TRUE;
    } else {							// xxx ,Dn ?
      switch (op4_3) {
      case 0: ope->op = (op8) ? OP_asl : OP_asr; break;
      case 1: ope->op = (op8) ? OP_lsl : OP_lsr; break;
      case 2: ope->op = (op8) ? OP_roxl : OP_roxr; break;
      case 3: ope->op = (op8) ? OP_rol : OP_ror; break;
      }
      if (op5) {						// xxx Dx,Dn
	ope->eas = ea2; ope->eas.imode = EA_D;
	ope->eas.isize = 1;
	ope->ead = ea1; ope->ead.imode = EA_D;
      } else {							// xxx #,Dn
	ope->eas.imode = EA_CST;
	ope->eas.data.b[0] = (op11_9 == 0) ? 8 : op11_9;
	ope->reas = FALSE;
	ope->ead = ea1; ope->ead.imode = EA_D;
      }
    }
    break;
  case 0xF:                                                  	// LINEF
    ope->vector = 0x0B;
    break;
  }
  if (ill_op) ope->vector = 4;		                        // Illegal instruction
  if (ope->eas.imode == EA_NONE) ope->reas = FALSE;
  if (ope->ead.imode == EA_NONE) { ope->read = FALSE; ope->wead = FALSE;}
}

