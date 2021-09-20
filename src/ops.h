/*
 *   Ops.h
 *
 *	 Real code for 68000 opcodes
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

#define SB ((ope->eas.data.b[0] & 0x80) ? 1 : 0)
#define SW ((ope->eas.data.w[0] & 0x8000) ? 1 : 0)
#define SL ((ope->eas.data.l & 0x80000000) ? 1 : 0)

#define DB ((ope->ead.data.b[0] & 0x80) ? 1 : 0)
#define DW ((ope->ead.data.w[0] & 0x8000) ? 1 : 0)
#define DL ((ope->ead.data.l & 0x80000000) ? 1 : 0)

#define RB ((ope->result.b[0] & 0x80) ? 1 : 0)
#define RW ((ope->result.w[0] & 0x8000) ? 1 : 0)
#define RL ((ope->result.l & 0x80000000) ? 1 : 0)

#define RZB ((ope->result.b[0] == 0x00) ? 1 : 0)
#define RZW	((ope->result.w[0] == 0x0000) ? 1 : 0)
#define RZL ((ope->result.l == 0x00000000)? 1 : 0)

BOOL checkcc(BYTE cc) {
	switch (cc) {
		case  0: return 1; break;
		case  1: return 0; break;
		case  2: return !Chipset.Cpu.SR.C && !Chipset.Cpu.SR.Z; break;
		case  3: return Chipset.Cpu.SR.C || Chipset.Cpu.SR.Z; break;
		case  4: return !Chipset.Cpu.SR.C; break;
		case  5: return Chipset.Cpu.SR.C; break;
		case  6: return !Chipset.Cpu.SR.Z; break;
		case  7: return Chipset.Cpu.SR.Z; break;
		case  8: return !Chipset.Cpu.SR.V; break;
		case  9: return Chipset.Cpu.SR.V; break;
		case 10: return !Chipset.Cpu.SR.N; break;
		case 11: return Chipset.Cpu.SR.N; break;
		case 12: return (Chipset.Cpu.SR.N && Chipset.Cpu.SR.V) || (!Chipset.Cpu.SR.N && !Chipset.Cpu.SR.V); break;
		case 13: return (Chipset.Cpu.SR.N && !Chipset.Cpu.SR.V) || (!Chipset.Cpu.SR.N && Chipset.Cpu.SR.V); break;
		case 14: return (Chipset.Cpu.SR.N && Chipset.Cpu.SR.V && !Chipset.Cpu.SR.Z) || (!Chipset.Cpu.SR.N && !Chipset.Cpu.SR.V && !Chipset.Cpu.SR.Z); break;
		default: return (Chipset.Cpu.SR.N && !Chipset.Cpu.SR.V) || (!Chipset.Cpu.SR.N && Chipset.Cpu.SR.V) || Chipset.Cpu.SR.Z; break;
	}
}

// MOVEC Rc,Rn
BYTE OP_moveccn(OP *ope) {		
//	BYTE status;

  fprintf(stderr,"MOVEC instruction");
  Chipset.Cpu.State = HALT;
  
  return BUS_OK;
}
// MOVEC Rn,Rc
BYTE OP_movecnc(OP *ope) {		
//	BYTE status;

  fprintf(stderr,"MOVEC instruction");
  Chipset.Cpu.State = HALT;
  
  return BUS_OK;
}
// MOVES
BYTE OP_moves(OP *ope) {		
//	BYTE status;
	
  fprintf(stderr,"MOVES instruction");
  Chipset.Cpu.State = HALT;

  return BUS_OK;
}
// MOVEP Dx,(d,Ay)
BYTE OP_movep_1(OP *ope) {		
	BYTE status;

	if (ope->eas.isize == 4) {
		status = WriteMEM(&ope->eas.data.b[3], ope->ead.addr.l, 1);
		Chipset.dcycles += 4;
		if (status != BUS_OK) return status;
		ope->ead.addr.l += 2;
		status = WriteMEM(&ope->eas.data.b[2], ope->ead.addr.l, 1);
		Chipset.dcycles += 4;
		if (status != BUS_OK) return status;
		ope->ead.addr.l += 2;
	}
	status = WriteMEM(&ope->eas.data.b[1], ope->ead.addr.l, 1);
	Chipset.dcycles += 4;
	if (status != BUS_OK) return status;
	ope->ead.addr.l += 2;
	status = WriteMEM(&ope->eas.data.b[0], ope->ead.addr.l, 1);
	Chipset.dcycles += 4;
	return status;
}
// MOVEP (d,Ay),Dx
BYTE OP_movep_2(OP *ope) {					
	BYTE status;

	if (ope->eas.isize == 4) {
		status = ReadMEM(&ope->result.b[3], ope->eas.addr.l, 1);
		Chipset.dcycles += 4;
		if (status != BUS_OK) return status;
		ope->eas.addr.l += 2;
		status = ReadMEM(&ope->result.b[2], ope->eas.addr.l, 1);
		Chipset.dcycles += 4;
		if (status != BUS_OK) return status;
		ope->eas.addr.l += 2;
	}
	status = ReadMEM(&ope->result.b[1], ope->eas.addr.l, 1);
	Chipset.dcycles += 4;
	if (status != BUS_OK) return status;
	ope->eas.addr.l += 2;
	status = ReadMEM(&ope->result.b[0], ope->eas.addr.l, 1);
	Chipset.dcycles += 4;
	return status;
}
// BTST 
BYTE OP_btst(OP *ope) {						
	if (ope->ead.imode == EA_D) 				// BTST ,Dn
		ope->eas.data.b[0] &= 0x1F;
	else 
		ope->eas.data.b[0] &= 0x07;
	Chipset.Cpu.SR.Z = (ope->ead.data.l & (1 << ope->eas.data.b[0])) ? 0 : 1;
	if (ope->eas.imode == EA_D) {				     // dynamic
		if (ope->ead.imode == EA_D) Chipset.dcycles += 2;
	} else	Chipset.dcycles += (ope->ead.imode == EA_D) ? 6 : 4; // static
	return BUS_OK;
}
// BCHG
BYTE OP_bchg(OP *ope) {
	if (ope->ead.imode == EA_D) 				// BCHG ,Dn
		ope->eas.data.b[0] &= 0x1F;
	else 
		ope->eas.data.b[0] &= 0x07;
	Chipset.Cpu.SR.Z = (ope->ead.data.l & (1 << ope->eas.data.b[0])) ? 0 : 1;
	ope->result.l = ope->ead.data.l ^ (1 << ope->eas.data.b[0]);
	Chipset.dcycles += (ope->eas.imode == EA_D) ? 4 : 8;		// dynamic / static
	return BUS_OK;
}

// BCLR
BYTE OP_bclr(OP *ope) {
	if (ope->ead.imode == EA_D) 				
		ope->eas.data.b[0] &= 0x1F;
	else 
		ope->eas.data.b[0] &= 0x07;
	Chipset.Cpu.SR.Z = (ope->ead.data.l & (1 << ope->eas.data.b[0])) ? 0 : 1;
	ope->result.l = ope->ead.data.l & (~(1 << ope->eas.data.b[0]));
	if (ope->eas.imode == EA_D) 								// dynamic
		Chipset.dcycles += (ope->ead.imode == EA_D) ? 6 : 4;
	else														// static
		Chipset.dcycles += (ope->ead.imode == EA_D) ? 10 : 8;
	return BUS_OK;
}

// BSET
BYTE OP_bset(OP *ope) {						
	if (ope->ead.imode == EA_D) 				// BSET ,Dn
		ope->eas.data.b[0] &= 0x1F;
	else 
		ope->eas.data.b[0] &= 0x07;
	Chipset.Cpu.SR.Z = (ope->ead.data.l & (1 << ope->eas.data.b[0])) ? 0 : 1;
	ope->result.l = ope->ead.data.l | (1 << ope->eas.data.b[0]);
	Chipset.dcycles += (ope->eas.imode == EA_D) ? 4 : 8;		// dynamic / static
	return BUS_OK;
}
// OR
BYTE OP_or(OP *ope) {
	ope->result.l = ope->eas.data.l | ope->ead.data.l;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	switch(ope->eas.isize) {
		case 1: Chipset.Cpu.SR.Z = RZB; break;
		case 2: Chipset.Cpu.SR.Z = RZW; break;
		default: Chipset.Cpu.SR.Z = RZL; break;
	}
	switch(ope->eas.isize) { 
		case 1: Chipset.Cpu.SR.N = RB; break;
		case 2: Chipset.Cpu.SR.N = RW; break;
		default: Chipset.Cpu.SR.N = RL; break;
	}
	if (ope->eas.isize == 4) {
		if (ope->ead.imode == EA_D) {
			if (ope->eas.imode == EA_IMM)
				Chipset.dcycles += 4;
			else Chipset.dcycles += 2;
		} else Chipset.dcycles += 8;
	} else if (ope->ead.imode != EA_D) Chipset.dcycles += 4;
	return BUS_OK;
}
// AND
BYTE OP_and(OP *ope) {
	ope->result.l = ope->eas.data.l & ope->ead.data.l;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	switch(ope->eas.isize) {
		case 1: Chipset.Cpu.SR.Z = RZB; break;
		case 2: Chipset.Cpu.SR.Z = RZW; break;
		default: Chipset.Cpu.SR.Z = RZL; break;
	}
	switch(ope->eas.isize) {
		case 1: Chipset.Cpu.SR.N = RB; break;
		case 2: Chipset.Cpu.SR.N = RW; break;
		default: Chipset.Cpu.SR.N = RL; break;
	}
	if (ope->eas.isize == 4) {
		if (ope->ead.imode == EA_D) {
			if (ope->eas.imode == EA_D)
				Chipset.dcycles += 4;
			else Chipset.dcycles += 2;
		} else Chipset.dcycles += 8;
	} else if (ope->ead.imode != EA_D) Chipset.dcycles += 4;
	return BUS_OK;
}
// EOR
BYTE OP_eor(OP *ope) {
	ope->result.l = ope->eas.data.l ^ ope->ead.data.l;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	switch(ope->eas.isize) {
		case 1: Chipset.Cpu.SR.Z = RZB; break;
		case 2: Chipset.Cpu.SR.Z = RZW; break;
		default: Chipset.Cpu.SR.Z = RZL; break;
	}
	switch(ope->eas.isize) {
		case 1: Chipset.Cpu.SR.N = RB; break;
		case 2: Chipset.Cpu.SR.N = RW; break;
		default: Chipset.Cpu.SR.N = RL; break;
	}
	if (ope->eas.isize == 4) {
		if (ope->ead.imode == EA_D) {
			Chipset.dcycles += 4;
		} else Chipset.dcycles += 8;
	} else if (ope->ead.imode != EA_D) Chipset.dcycles += 4;
	return BUS_OK;
}
// ADD
BYTE OP_add(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = ope->eas.data.b[0] + ope->ead.data.b[0];
			Chipset.Cpu.SR.C = ((SB && DB) || (!RB && DB) || (SB && !RB)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((SB && DB && !RB) || (!SB && !DB && RB)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = ope->eas.data.w[0] + ope->ead.data.w[0];
			Chipset.Cpu.SR.C = ((SW && DW) || (!RW && DW) || (SW && !RW)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((SW && DW && !RW) || (!SW && !DW && RW)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = ope->eas.data.l + ope->ead.data.l;
			Chipset.Cpu.SR.C = ((SL && DL) || (!RL && DL) || (SL && !RL)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((SL && DL && !RL) || (!SL && !DL && RL)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if (ope->eas.isize == 4) {
		if (ope->ead.imode == EA_D) {
			if ((ope->eas.imode == EA_D) || (ope->eas.imode == EA_A) || (ope->eas.imode == EA_CST) || (ope->eas.imode == EA_IMM))
				Chipset.dcycles += 4;
			else Chipset.dcycles += 2;
		} else Chipset.dcycles += 8;
	} else if (ope->ead.imode != EA_D) Chipset.dcycles += 4;
	return BUS_OK;
}
// ADDA Ea, An
BYTE OP_adda(OP *ope) {
	if (ope->eas.isize == 2)
		ope->eas.data.w[1] = (ope->eas.data.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	ope->result.l = ope->eas.data.l + ope->ead.data.l;
	if (ope->eas.isize == 4) {
		if ((ope->eas.imode == EA_D) || (ope->eas.imode == EA_A) || (ope->eas.imode == EA_CST) || (ope->eas.imode == EA_IMM))
			Chipset.dcycles += 4;
		else Chipset.dcycles += 2;
	} else if (ope->eas.imode != EA_CST) Chipset.dcycles += 4;
	return BUS_OK;
}
// ADDX
BYTE OP_addx(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = ope->eas.data.b[0] + ope->ead.data.b[0] + Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = ((SB && DB) || (!RB && DB) || (SB && !RB)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((SB && DB && !RB) || (!SB && !DB && RB)) ? 1 : 0;
			if (ope->result.b[0] != 0x00) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = ope->eas.data.w[0] + ope->ead.data.w[0] + Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = ((SW && DW) || (!RW && DW) || (SW && !RW)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((SW && DW && !RW) || (!SW && !DW && RW)) ? 1 : 0;
			if (ope->result.w[0] != 0x0000) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = ope->eas.data.l + ope->ead.data.l + Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = ((SL && DL) || (!RL && DL) || (SL && !RL)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((SL && DL && !RL) || (!SL && !DL && RL)) ? 1 : 0;
			if (ope->result.l != 0x00000000) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if (ope->eas.isize == 4) 
		Chipset.dcycles += (ope->eas.imode == EA_D) ? 4 : 6; // not really
	else if (ope->eas.imode != EA_D) Chipset.dcycles += 2;
	return BUS_OK;
}
// SUB
BYTE OP_sub(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = ope->ead.data.b[0] - ope->eas.data.b[0];
			Chipset.Cpu.SR.C = ((SB && !DB) || (RB && !DB) || (SB && RB)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SB && DB && !RB) || (SB && !DB && RB)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = ope->ead.data.w[0] - ope->eas.data.w[0];
			Chipset.Cpu.SR.C = ((SW && !DW) || (RW && !DW) || (SW && RW)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SW && DW && !RW) || (SW && !DW && RW)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = ope->ead.data.l - ope->eas.data.l;
			Chipset.Cpu.SR.C = ((SL && !DL) || (RL && !DL) || (SL && RL)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SL && DL && !RL) || (SL && !DL && RL)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if (ope->eas.isize == 4) {
		if (ope->ead.imode == EA_D) {
			if ((ope->eas.imode == EA_D) || (ope->eas.imode == EA_A) || (ope->eas.imode == EA_CST) || (ope->eas.imode == EA_IMM))
				Chipset.dcycles += 4;
			else Chipset.dcycles += 2;
		} else Chipset.dcycles += 8;
	} else if (ope->ead.imode != EA_D) Chipset.dcycles += 4;
	return BUS_OK;
}
// SUBA
BYTE OP_suba(OP *ope) {
	if (ope->eas.isize == 2) 
		ope->eas.data.w[1] = (ope->eas.data.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	ope->result.l = ope->ead.data.l - ope->eas.data.l;
	if (ope->eas.isize == 4) {
		if ((ope->eas.imode == EA_D) || (ope->eas.imode == EA_A) || (ope->eas.imode == EA_CST) || (ope->eas.imode == EA_IMM))
			Chipset.dcycles += 4;
		else Chipset.dcycles += 2;
	} else Chipset.dcycles += 4;
	return BUS_OK;
}
// SUBX
BYTE OP_subx(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = ope->ead.data.b[0] - ope->eas.data.b[0] - Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = ((SB && !DB) || (RB && !DB) || (SB && RB)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SB && DB && !RB) || (SB && !DB && RB)) ? 1 : 0;
			if (ope->result.b[0] != 0x00) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = ope->ead.data.w[0] - ope->eas.data.w[0] - Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = ((SW && !DW) || (RW && !DW) || (SW && RW)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SW && DW && !RW) || (SW && !DW && RW)) ? 1 : 0;
			if (ope->result.w[0] != 0x0000) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = ope->ead.data.l - ope->eas.data.l - Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = ((SL && !DL) || (RL && !DL) || (SL && RL)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SL && DL && !RL) || (SL && !DL && RL)) ? 1 : 0;
			if (ope->result.l != 0x00000000) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if (ope->eas.isize == 4) 
		Chipset.dcycles += (ope->eas.imode == EA_D) ? 4 : -2;
	else if (ope->eas.imode != EA_D) Chipset.dcycles -= 2;
	return BUS_OK;
}

// ABCD
BYTE OP_abcd(OP *ope) {
	BYTE a,b;
	a = (ope->eas.data.b[0] & 0xf) + (ope->ead.data.b[0] & 0xf) + Chipset.Cpu.SR.X;
	if (a > 9) {
		a -= 10;
		Chipset.Cpu.SR.C = 1; 
	} else Chipset.Cpu.SR.C = 0;
	b = (ope->eas.data.b[0] >> 4) + (ope->ead.data.b[0] >> 4) + Chipset.Cpu.SR.C;
	if (b > 9) {
		b -= 10;
		Chipset.Cpu.SR.C = 1; 
	} else Chipset.Cpu.SR.C = 0;
	ope->result.b[0] = ((b & 0x0F) << 4) | (a & 0x0F);
	if (ope->result.b[0] != 0x00) Chipset.Cpu.SR.Z = 0;
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.dcycles += (ope->eas.imode == EA_D) ? 2 : -2;
	return BUS_OK;
}
// SBCD
BYTE OP_sbcd(OP *ope) {
	signed char a,b;
	a = (ope->ead.data.b[0] & 0xf) - (ope->eas.data.b[0] & 0xf) - Chipset.Cpu.SR.X;
	if (a < 0) {
		a += 10;
		Chipset.Cpu.SR.C = 1; 
	} else Chipset.Cpu.SR.C = 0;
	b = (ope->ead.data.b[0] >> 4) - (ope->eas.data.b[0] >> 4) - Chipset.Cpu.SR.C;
	if (b < 0) {
		b += 10;
		Chipset.Cpu.SR.C = 1; 
	} else Chipset.Cpu.SR.C = 0;
	ope->result.b[0] = ((b & 0x0F) << 4) | (a & 0x0F);
	if (ope->result.b[0] != 0x00) Chipset.Cpu.SR.Z = 0;
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.dcycles += (ope->eas.imode == EA_D) ? 2 : -2;
	return BUS_OK;
}

// CMP Ea,Dn ; CMPM (An)+,(An)+
BYTE OP_cmp(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = ope->ead.data.b[0] - ope->eas.data.b[0];
			Chipset.Cpu.SR.C = ((SB && !DB) || (RB && !DB) || (SB && RB)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SB && DB && !RB) || (SB && !DB && RB)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = ope->ead.data.w[0] - ope->eas.data.w[0];
			Chipset.Cpu.SR.C = ((SW && !DW) || (RW && !DW) || (SW && RW)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SW && DW && !RW) || (SW && !DW && RW)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = ope->ead.data.l - ope->eas.data.l;
			Chipset.Cpu.SR.C = ((SL && !DL) || (RL && !DL) || (SL && RL)) ? 1 : 0;
			Chipset.Cpu.SR.V = ((!SL && DL && !RL) || (SL && !DL && RL)) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	if (ope->ead.isize == 4) Chipset.dcycles += 2; 
	return BUS_OK;
}
// CMPA Ea,An
BYTE OP_cmpa(OP *ope) {
	if (ope->eas.isize == 2) 
		ope->eas.data.w[1] = (ope->eas.data.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	ope->result.l = ope->ead.data.l - ope->eas.data.l;
	Chipset.Cpu.SR.C = ((SL && !DL) || (RL && !DL) || (SL && RL)) ? 1 : 0;
	Chipset.Cpu.SR.V = ((!SL && DL && !RL) || (SL && !DL && RL)) ? 1 : 0;
	Chipset.Cpu.SR.Z = RZL;
	Chipset.Cpu.SR.N = RL;
	Chipset.dcycles += 2; 
	return BUS_OK;
}
// MOVE.B Ea,Ea
BYTE OP_moveb(OP *ope) {										
	ope->result.b[0] = ope->eas.data.b[0];
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.Z = RZB;
	Chipset.Cpu.SR.N = RB;
	if (ope->ead.imode == EA_IAD) Chipset.dcycles -= 2;		// correct for destination
	return BUS_OK;
}
// MOVE.W Ea,Ea
BYTE OP_movew(OP *ope) {										
	ope->result.w[0] = ope->eas.data.w[0];
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	if (ope->ead.imode == EA_IAD) Chipset.dcycles -= 2;		// correct for destination
	return BUS_OK;
}
// MOVE.L Ea,Ea
BYTE OP_movel(OP *ope) {										
	ope->result.l = ope->eas.data.l;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.Z = RZL;
	Chipset.Cpu.SR.N = RL;
	if (ope->ead.imode == EA_IAD) Chipset.dcycles -= 2;		// correct for destination
	return BUS_OK;
}
// MOVEA.W Ea,An
BYTE OP_moveaw(OP *ope) {										
	ope->result.w[0] = ope->eas.data.w[0];
	ope->result.w[1] = (ope->result.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	return BUS_OK;
}
// MOVEA.L Ea,An
BYTE OP_moveal(OP *ope) {										
	ope->result.l = ope->eas.data.l;
	return BUS_OK;
}
// LEA EA,An
BYTE OP_lea(OP *ope) {					
	ope->result.l = ope->eas.addr.l;
	if ((ope->eas.imode == EA_IDPX) || (ope->eas.imode == EA_IDAX))
		Chipset.dcycles += 2;
	return BUS_OK;
}
// CHK(.W) Ea,Dn
BYTE OP_chk(OP *ope) {					
	if (ope->ead.data.w[0] & 0x8000) {			// Dn < 0
		Chipset.Cpu.SR.N = 1;
		ope->vector = 6;
	} else if ((signed short) (ope->ead.data.w[0]) > (signed short) (ope->eas.data.w[0])) { // Dn > Ea
		Chipset.Cpu.SR.N = 0;
		ope->vector = 6;						// chk exception
	}
	Chipset.dcycles += 6;
	return BUS_OK;
}
// NEGX Ea
BYTE OP_negx(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = 0 - ope->eas.data.b[0] - Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = (SB || RB) ? 1 : 0;
			Chipset.Cpu.SR.V = (SB && RB) ? 1 : 0;
			if (ope->result.b[0] != 0x00) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = 0 - ope->eas.data.w[0] - Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = (SW || RW) ? 1 : 0;
			Chipset.Cpu.SR.V = (SW && RW) ? 1 : 0;
			if (ope->result.w[0] != 0x0000) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = 0 - ope->eas.data.l - Chipset.Cpu.SR.X;
			Chipset.Cpu.SR.C = (SL || RL) ? 1 : 0;
			Chipset.Cpu.SR.V = (SL && RL) ? 1 : 0;
			if (ope->result.l != 0x00000000) Chipset.Cpu.SR.Z = 0;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if ((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D))
		Chipset.dcycles += (ope->eas.isize == 4) ? 2 : 0 ;
	else Chipset.dcycles += (ope->eas.isize == 4) ? 8 : 4 ;
	return BUS_OK;
}
// NEG Ea
BYTE OP_neg(OP *ope) {
	switch (ope->eas.isize) {
		case 1:
			ope->result.b[0] = 0 - ope->eas.data.b[0];
			Chipset.Cpu.SR.C = (SB || RB) ? 1 : 0;
			Chipset.Cpu.SR.V = (SB && RB) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			ope->result.w[0] = 0 - ope->eas.data.w[0];
			Chipset.Cpu.SR.C = (SW || RW) ? 1 : 0;
			Chipset.Cpu.SR.V = (SW && RW) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			ope->result.l = 0 - ope->eas.data.l;
			Chipset.Cpu.SR.C = (SL || RL) ? 1 : 0;
			Chipset.Cpu.SR.V = (SL && RL) ? 1 : 0;
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if ((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D))
		Chipset.dcycles += (ope->eas.isize == 4) ? 2 : 0 ;
	else Chipset.dcycles += (ope->eas.isize == 4) ? 8 : 4 ;
	return BUS_OK;
}
// CLR Ea
BYTE OP_clr(OP *ope) {					
	ope->result.l = 0x00000000;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.Z = 1;
	Chipset.Cpu.SR.N = 0;
	if ((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D)) {
		if (ope->eas.isize == 4) Chipset.dcycles += 2;
	} else Chipset.dcycles += (ope->eas.isize == 4) ? 8 : 4 ;
	return BUS_OK;
}
// NOT Ea
BYTE OP_not(OP *ope) {
	ope->result.l = ~ope->eas.data.l;
	switch (ope->eas.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	if ((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D)) {
		if (ope->eas.isize == 4) Chipset.dcycles += 2;
	} else Chipset.dcycles += (ope->eas.isize == 4) ? 8 : 4 ;
	return BUS_OK;
}
// ORI to SR
BYTE OP_orisr(OP *ope) {
	ope->result.w[0] = ope->eas.data.w[0] | ope->ead.data.w[0];
	Chipset.dcycles += 12;
	return BUS_OK;
}
// ANDI to SR
BYTE OP_andisr(OP *ope) {
	ope->result.w[0] = ope->eas.data.w[0] & ope->ead.data.w[0];
	Chipset.dcycles += 12;
	return BUS_OK;
}
// EORI to SR
BYTE OP_eorisr(OP *ope) {
	ope->result.w[0] = ope->eas.data.w[0] ^ ope->ead.data.w[0];
	Chipset.dcycles += 12;
	return BUS_OK;
}
// ORI to CR
BYTE OP_oricr(OP *ope) {
	ope->result.b[0] = ope->eas.data.b[0] | ope->ead.data.b[0];
	Chipset.dcycles += 12;
	return BUS_OK;
}
// ANDI to CR
BYTE OP_andicr(OP *ope) {
	ope->result.b[0] = ope->eas.data.b[0] & ope->ead.data.b[0];
	Chipset.dcycles += 12;
	return BUS_OK;
}
// EORI to CR
BYTE OP_eoricr(OP *ope) {
	ope->result.b[0] = ope->eas.data.b[0] ^ ope->ead.data.b[0];
	Chipset.dcycles += 12;
	return BUS_OK;
}
// MOVE from to SR
BYTE OP_movesr(OP *ope) {				
	ope->result.w[0] = ope->eas.data.w[0];
	if (ope->eas.imode == EA_SR) {
		Chipset.dcycles += ((ope->ead.imode == EA_A) || (ope->ead.imode == EA_A)) ? 2 : 4;
	} else Chipset.dcycles += 8;
	return BUS_OK;
}
// MOVE from to CCR
BYTE OP_moveccr(OP *ope) {				
	ope->result.b[0] = ope->eas.data.b[0];
	Chipset.dcycles += 8;
	return BUS_OK;
}

// NBCD Ea
BYTE OP_nbcd(OP *ope) {
	signed char a,b;
	a = 0 - (ope->eas.data.b[0] & 0xf) - Chipset.Cpu.SR.X;
	if (a < 0) {
		a += 10;
		Chipset.Cpu.SR.C = 1; 
	} else Chipset.Cpu.SR.C = 0;
	b = 0 - (ope->eas.data.b[0] >> 4) - Chipset.Cpu.SR.C;
	if (b < 0) {
		b += 10;
		Chipset.Cpu.SR.C = 1; 
	} else Chipset.Cpu.SR.C = 0;
	ope->result.b[0] = ((b & 0x0F) << 4) | (a & 0x0F);
	if (ope->result.b[0] != 0x00) Chipset.Cpu.SR.Z = 0;
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	if ((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D))
		Chipset.dcycles += 2;
	else Chipset.dcycles += 4;
	return BUS_OK;
}
// SWAP
BYTE OP_swap(OP *ope) {
	ope->result.w[0] = ope->eas.data.w[1];
	ope->result.w[1] = ope->eas.data.w[0];
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.Z = RZL;
	Chipset.Cpu.SR.N = RL;
	return BUS_OK;
}
// PEA Ea
BYTE OP_pea(OP *ope) {
	BYTE status;
	DEC4SP;
	status = PUSHL(ope->eas.addr.l);
	if ((ope->eas.imode == EA_IDPX) || (ope->eas.imode == EA_IDAX))
		Chipset.dcycles += 10;
	else Chipset.dcycles += 8;
	return status;
}
// EXT
BYTE OP_ext(OP *ope) {
	if (ope->eas.isize == 2) {
		ope->result.b[0] = ope->eas.data.b[0];
		ope->result.b[1] = (ope->result.b[0] & 0x80) ? 0xFF : 0x00;
		Chipset.Cpu.SR.N = RW;
		Chipset.Cpu.SR.Z = RZW;
 	} else {
		ope->result.w[0] = ope->eas.data.w[0];
		ope->result.w[1] = (ope->result.w[0] & 0x8000) ? 0xFFFF : 0x0000;
		Chipset.Cpu.SR.N = RL;
		Chipset.Cpu.SR.Z = RZL;
	}
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	return BUS_OK;
}

// MOVEM L,Ea
BYTE OP_movemlea(OP *ope) {
	BYTE i, status;
	MEM *reg;

	if (ope->eas.data.w[0] != 0x0000) {
		Chipset.dcycles -= 4;
		if (ope->ead.imode == EA_IAD) {					// reverse mode
			if (Chipset.Cpu.SR.S) reg = & Chipset.Cpu.A[8];	// SSP
			else reg = & Chipset.Cpu.A[7];					// USP
		} else reg = & Chipset.Cpu.D[0];					// normal mode

		for (i = 0; i < 16; i++) {
			if (ope->eas.data.w[0] & 0x0001) {
				status = WriteMEM((BYTE *) reg, ope->ead.addr.l, ope->ead.isize);	// same for W and L (big endian)
				Chipset.dcycles += (ope->ead.isize == 4) ? 8 : 4;
				if (status != BUS_OK) return status;
				if (ope->ead.imode == EA_IAD)
					ope->ead.addr.l -= ope->ead.isize; 
				else
					ope->ead.addr.l += ope->ead.isize; 
			}
			ope->eas.data.w[0] = ope->eas.data.w[0] >> 1;
			if (ope->ead.imode == EA_IAD) {					// reverse mode
				reg -= 1;										// previous reg
				if ((i == 0) && Chipset.Cpu.SR.S) reg -= 1;			// A7 was SSP, so next is A6
			} else {
				reg += 1;									// next reg
				if ((i == 14) && Chipset.Cpu.SR.S) reg += 1;			// current is A6, next is SSP
			}
		}
		if (ope->ead.imode == EA_IAD) {							// -(An), store addr back to reg
			if ((ope->ead.reg == 7) && Chipset.Cpu.SR.S) {
				Chipset.Cpu.A[8].l = ope->ead.addr.l + ope->ead.isize;
			} else {
				Chipset.Cpu.A[ope->ead.reg].l = ope->ead.addr.l + ope->ead.isize;
			}
		}
	}
	return BUS_OK;
}
// MOVEM Ea,L
BYTE OP_movemeal(OP *ope) {
	BYTE i, status;
	MEM *reg;
	MEM data;

	if (ope->eas.data.w[0] != 0x0000) {
		reg = & Chipset.Cpu.D[0];						// D0
		for (i = 0; i < 16; i++) {
			if (ope->eas.data.w[0] & 0x0001) {
				status = ReadMEM((BYTE *) & data.l, ope->ead.addr.l, ope->ead.isize);	// same for W and L (endianness)
				if (ope->ead.isize == 2)
					data.w[1] = (data.w[0] & 0x8000) ? 0xFFFF : 0x0000; 
				Chipset.dcycles += (ope->ead.isize == 4) ? 8 : 4;
				if (status != BUS_OK) return status;
				reg->l = data.l;
				ope->ead.addr.l += ope->ead.isize;		
			}
			reg += 1;										// next reg
			if ((i == 14) && Chipset.Cpu.SR.S) reg += 1;			// A7 is SSP
			ope->eas.data.w[0] = ope->eas.data.w[0] >> 1;
		}
		if (ope->ead.imode == EA_IAI) {							// (An)+, store addr back to reg
			if ((ope->ead.reg == 7) && Chipset.Cpu.SR.S) 
				Chipset.Cpu.A[8].l = ope->ead.addr.l;
			else 
				Chipset.Cpu.A[ope->ead.reg].l = ope->ead.addr.l;
		}
		Chipset.dcycles += 4;
	}
	return BUS_OK;
}
// TST Ea
BYTE OP_tst(OP *ope) {
	ope->result.l = ope->eas.data.l;		// just for the tests ...
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	switch(ope->eas.isize) {
		case 1: Chipset.Cpu.SR.Z = RZB; break;
		case 2: Chipset.Cpu.SR.Z = RZW; break;
		default: Chipset.Cpu.SR.Z = RZL; break;
	}
	switch(ope->eas.isize) { 
		case 1: Chipset.Cpu.SR.N = RB; break;
		case 2: Chipset.Cpu.SR.N = RW; break;
		default: Chipset.Cpu.SR.N = RL; break;
	}
	return BUS_OK;
}
// TAS Ea
BYTE OP_tas(OP *ope) {
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.Z = (ope->eas.data.b[0] == 0x00) ? 1 : 0;
	Chipset.Cpu.SR.N = (ope->eas.data.b[0] & 0x80) ? 1 : 0;
	ope->result.b[0] = 0x80 | ope->eas.data.b[0];
	if (!((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D)))
		Chipset.dcycles += 10;
	return BUS_OK;
}
// JMP Ea
BYTE OP_jmp(OP *ope) {
	Chipset.Cpu.PC = ope->eas.addr.l;
	if ((ope->eas.imode == EA_IDP) || (ope->eas.imode == EA_IXW) || (ope->eas.imode == EA_IDA)) 
		Chipset.dcycles += 2;
	else if (ope->eas.imode != EA_IXL)
		Chipset.dcycles += 4;
	return BUS_OK;
}
// JSR Ea
BYTE OP_jsr(OP *ope) {
	BYTE status;
	DEC4SP;
	status = PUSHL(Chipset.Cpu.PC);
	Chipset.dcycles += 8;
	if (status != BUS_OK) return status;
	Chipset.Cpu.PC = ope->eas.addr.l;
	if ((ope->eas.imode == EA_IDP) || (ope->eas.imode == EA_IXW) || (ope->eas.imode == EA_IDA)) 
		Chipset.dcycles += 2;
	else if (ope->eas.imode != EA_IXL)
		Chipset.dcycles += 4;
	return BUS_OK;
}
// TRAP #
BYTE OP_trap(OP *ope) {
	ope->vector = 32 + ope->eas.data.b[0];
	return BUS_OK;
}
// LINK An,#
BYTE OP_link(OP *ope) {
	BYTE status;
	DEC4SP;
	status = PUSHL(ope->eas.data.l);
	Chipset.dcycles += 8;
	if (status != BUS_OK) return status;
	if (Chipset.Cpu.SR.S) Chipset.Cpu.A[ope->eas.reg].l = Chipset.Cpu.A[8].l;
	else Chipset.Cpu.A[ope->eas.reg].l = Chipset.Cpu.A[7].l;
	ope->ead.data.w[1] = (ope->ead.data.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	if (Chipset.Cpu.SR.S) Chipset.Cpu.A[8].l += ope->ead.data.l;
	else Chipset.Cpu.A[7].l += ope->ead.data.l;
	return BUS_OK;
}
// UNLK An
BYTE OP_unlk(OP *ope) {
	BYTE status;
	if (Chipset.Cpu.SR.S) Chipset.Cpu.A[8].l = ope->eas.data.l;
	else Chipset.Cpu.A[7].l = ope->eas.data.l;
	status = POPL(ope->result.l);
	Chipset.dcycles += 8;
	if (status != BUS_OK) return status;
	INC4SP;
	return BUS_OK;
}
// RESET
BYTE OP_reset(OP *ope) {
	Chipset.Cpu.reset = 1;
	Chipset.dcycles += 128;
	return BUS_OK;
}
// NOP
BYTE OP_nop(OP *ope) {
	return BUS_OK;
}
// STOP #
BYTE OP_stop(OP *ope) {
	Chipset.Cpu.SR.sr = ope->eas.data.w[0] & 0xA71F;
	Chipset.Cpu.State = HALT;
	return BUS_OK;
}
// RTE
BYTE OP_rte(OP *ope) {
	BYTE status;
	WORD sr_copy;
#ifdef M68000
#else							// 68010 without any rerun
	WORD data;
	WORD format;
	WORD i;
#endif

	status = POPW(sr_copy);
	Chipset.dcycles += 4;
	if (status != BUS_OK) return status;
	INC2SP;
	status = POPL(Chipset.Cpu.PC);
	Chipset.dcycles += 8;
	if (status != BUS_OK) return status;
	INC4SP;
#ifdef M68000
#else							// 68010 without any rerun
	status = POPW(format);
	Chipset.dcycles += 4;
	if (status != BUS_OK) return status;
	INC2SP;
	switch (format >> 12) {
		case 0:						// format 0, done
			break;
		case 8:						// format 8, pop 25 more words
			for (i = 0; i < 25; i++) {
				status = POPW(data);
				Chipset.dcycles += 4;
				if (status != BUS_OK) return status;
				INC2SP;
			}
			break;
		default:
			ope->vector = 14;		// format error
			break;
	}
#endif

	Chipset.Cpu.SR.sr = sr_copy & 0xA71F;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// RTS
BYTE OP_rts(OP *ope) {
	BYTE status;
	status = POPL(Chipset.Cpu.PC);
	Chipset.dcycles += 8;
	if (status != BUS_OK) 
		return status;
	INC4SP;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// TRAPV
BYTE OP_trapv(OP *ope) {
	if (Chipset.Cpu.SR.V) ope->vector = 7;
	return BUS_OK;
}
// RTR
BYTE OP_rtr(OP *ope) {
	BYTE status;
	WORD data;

	status = POPW(data);
	Chipset.dcycles += 4;
	if (status != BUS_OK) return status;
	Chipset.Cpu.SR.r[0] = (data & 0x001F);
	INC2SP;
	status = POPL(Chipset.Cpu.PC);
	Chipset.dcycles += 8;
	if (status != BUS_OK) return status;
	INC4SP;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// DBCC
BYTE OP_dbcc(OP *ope) {
	if (!checkcc(ope->ead.data.b[0])) {
		Chipset.Cpu.D[ope->eas.reg].w[0] -= 1;
		if (Chipset.Cpu.D[ope->eas.reg].w[0] != 0xFFFF) {
			Chipset.Cpu.PC = ope->ead.addr.l;
			Chipset.dcycles += 2;
		} else Chipset.dcycles += 6;
	} else Chipset.dcycles += 4;
	return BUS_OK;
}
// SCC Ea
BYTE OP_scc(OP *ope) {
	if (checkcc(ope->eas.data.b[2])) {
		ope->result.b[0] = 0xFF;
		Chipset.dcycles += ((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D)) ? 2: 4;
	} else {
		ope->result.b[0] = 0x00;
		if (!((ope->eas.imode == EA_A) || (ope->eas.imode == EA_D))) Chipset.dcycles +=4;
	}
	return BUS_OK;
}
// BCC
BYTE OP_bcc(OP *ope) {
	if (checkcc(ope->eas.data.b[1])) {
		Chipset.Cpu.PC = ope->eas.addr.l;
		Chipset.dcycles += 6;
	} else {
		if (ope->eas.data.b[0] == 0x00) Chipset.dcycles += 4;
		else Chipset.dcycles += 8;
	}
	return BUS_OK;
}
// BRA
BYTE OP_bra(OP *ope) {
	Chipset.Cpu.PC = ope->eas.addr.l;
	Chipset.dcycles += 6;
	return BUS_OK;
}
// BSR
BYTE OP_bsr(OP *ope) {
	BYTE status;
	DEC4SP;
	status = PUSHL(Chipset.Cpu.PC);
	Chipset.dcycles += 8;
	if (status != BUS_OK) return status;
	Chipset.Cpu.PC = ope->eas.addr.l;
	Chipset.dcycles += 6;
	return BUS_OK;
}
// MOVEQ #,Dn
BYTE OP_moveq(OP *ope) {
	ope->result.b[0] = ope->eas.data.b[0];
	ope->result.b[1] = (ope->result.b[0] & 0x80) ? 0xFF : 0x00;
	ope->result.w[1] = (ope->result.w[0] & 0x8000) ? 0xFFFF : 0x0000;
	Chipset.Cpu.SR.Z = RZL;
	Chipset.Cpu.SR.N = RL;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.C = 0;
	return BUS_OK;
}
// DIVS
BYTE OP_divs(OP *ope) {
	ldiv_t res;

	Chipset.Cpu.SR.C = 0;
	if (ope->eas.data.w[0] == 0) {
		ope->vector = 5;
		return BUS_OK;
	}
	Chipset.dcycles += 10;	
	res = ldiv((signed long int)(ope->ead.data.sl), (signed long int)(ope->eas.data.sw[0]));
	if (abs(res.quot) > 0x00007FFF) {	// overflow
		Chipset.Cpu.SR.V = 1;
		ope->wead = FALSE;
		return BUS_OK;
	}
	ope->result.sw[0] = (signed short)(res.quot);
	ope->result.sw[1] = (signed short)(res.rem);
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 60;
	return BUS_OK;
}
// DIVU
BYTE OP_divu(OP *ope) {
	Chipset.Cpu.SR.C = 0;
	if (ope->eas.data.w[0] == 0) {
		ope->vector = 5;
		return BUS_OK;
	}
	if (ope->ead.data.w[1] >= ope->eas.data.w[0]) {		// overflow
		Chipset.Cpu.SR.V = 1;
		ope->wead = FALSE;
		return BUS_OK;
	}
	ope->result.w[0] = ope->ead.data.l / ope->eas.data.w[0];
	ope->result.w[1] = ope->ead.data.l % ope->eas.data.w[0];
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 70;
	return BUS_OK;
}
// MULU
BYTE OP_mulu(OP *ope) {
	ope->result.l = (ope->eas.data.w[0] * ope->ead.data.w[0]);
	Chipset.Cpu.SR.Z = RZL;
	Chipset.Cpu.SR.N = RL;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.C = 0;
	Chipset.dcycles += 38;
	return BUS_OK;
}
// MULS 
BYTE OP_muls(OP *ope) {
	ope->result.sl = (ope->eas.data.sw[0]) * (ope->ead.data.sw[0]);
	Chipset.Cpu.SR.Z = RZL;
	Chipset.Cpu.SR.N = RL;
	Chipset.Cpu.SR.V = 0;
	Chipset.Cpu.SR.C = 0;
	Chipset.dcycles += 38;
	return BUS_OK;
}
// EXG An,An
BYTE OP_exgaa(OP *ope) {
	if ((ope->eas.reg == 7) && Chipset.Cpu.SR.S) 
		Chipset.Cpu.A[8].l = ope->ead.data.l;
	else Chipset.Cpu.A[ope->eas.reg].l = ope->ead.data.l;
	ope->result.l = ope->eas.data.l;
	Chipset.dcycles += 2;
	return BUS_OK;
}
// EXG An,Dn
BYTE OP_exgad(OP *ope) {
	if ((ope->eas.reg == 7) && Chipset.Cpu.SR.S) 
		Chipset.Cpu.A[8].l = ope->ead.data.l;
	else Chipset.Cpu.A[ope->eas.reg].l = ope->ead.data.l;
	ope->result.l = ope->eas.data.l;
	Chipset.dcycles += 2;
	return BUS_OK;
}
// EXG Dn,Dn
BYTE OP_exgdd(OP *ope) {
	Chipset.Cpu.D[ope->eas.reg].l = ope->ead.data.l;
	ope->result.l = ope->eas.data.l;
	Chipset.dcycles += 2;
	return BUS_OK;
}
// ASL #,Dn
BYTE OP_asl(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x80) ? 1 : 0;
				ope->result.b[0] = ope->result.b[0] << 1;
				if (Chipset.Cpu.SR.C != ((ope->result.b[0] & 0x80) ? 1 : 0))
					Chipset.Cpu.SR.V = 1;
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x8000) ? 1 : 0;
				ope->result.w[0] = ope->result.w[0] << 1;
				if (Chipset.Cpu.SR.C != ((ope->result.w[0] & 0x8000) ? 1 : 0))
					Chipset.Cpu.SR.V = 1;
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x80000000) ? 1 : 0;
				ope->result.l = ope->result.l << 1;
				if (Chipset.Cpu.SR.C != ((ope->result.l & 0x80000000) ? 1 : 0))
					Chipset.Cpu.SR.V = 1;
				break;
		}
		Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.dcycles += 2; 
	return BUS_OK;
}
// ASR
BYTE OP_asr(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x01) ? 1 : 0;
				ope->result.b[0] = (ope->result.b[0] & 0x80) | (ope->result.b[0] >> 1);
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x0001) ? 1 : 0;
				ope->result.w[0] = (ope->result.w[0] & 0x8000) | (ope->result.w[0] >> 1);
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x00000001) ? 1 : 0;
				ope->result.l = (ope->result.l & 0x80000000) | (ope->result.l >> 1);
				break;
		}
		Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.dcycles += 2; 
	return BUS_OK;
}

BYTE OP_lsl(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x80) ? 1 : 0;
				ope->result.b[0] = ope->result.b[0] << 1;
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x8000) ? 1 : 0;
				ope->result.w[0] = ope->result.w[0] << 1;
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x80000000) ? 1 : 0;
				ope->result.l = ope->result.l << 1;
				break;
		}
		Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.dcycles += 2; 
	return BUS_OK;
}

BYTE OP_lsr(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x01) ? 1 : 0;
				ope->result.b[0] = ope->result.b[0] >> 1;
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x0001) ? 1 : 0;
				ope->result.w[0] = ope->result.w[0] >> 1;
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x00000001) ? 1 : 0;
				ope->result.l = ope->result.l >> 1;
				break;
		}
		Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.dcycles += 2; 
	return BUS_OK;
}

BYTE OP_roxl(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x80) ? 1 : 0;
				ope->result.b[0] = (ope->result.b[0] << 1) | Chipset.Cpu.SR.X;
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x8000) ? 1 : 0;
				ope->result.w[0] = (ope->result.w[0] << 1) | Chipset.Cpu.SR.X;
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x80000000) ? 1 : 0;
				ope->result.l = (ope->result.l << 1) | Chipset.Cpu.SR.X;
				break;
		}
		Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.C = Chipset.Cpu.SR.X;
	Chipset.dcycles += 2; 
	return BUS_OK;
}

BYTE OP_roxr(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x01) ? 1 : 0;
				ope->result.b[0] = (ope->result.b[0] >> 1) | (Chipset.Cpu.SR.X ? 0x80 : 0x00);
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x0001) ? 1 : 0;
				ope->result.w[0] = (ope->result.w[0] >> 1) | (Chipset.Cpu.SR.X ? 0x8000 : 0x0000);
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x00000001) ? 1 : 0;
				ope->result.l = (ope->result.l >> 1) | (Chipset.Cpu.SR.X ? 0x80000000 : 0x00000000);
				break;
		}
		Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.Cpu.SR.C = Chipset.Cpu.SR.X;
	Chipset.dcycles += 2; 
	return BUS_OK;
}

BYTE OP_rol(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x80) ? 1 : 0;
				ope->result.b[0] = (ope->result.b[0] << 1) | Chipset.Cpu.SR.C;
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x8000) ? 1 : 0;
				ope->result.w[0] = (ope->result.w[0] << 1) | Chipset.Cpu.SR.C;
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x80000000) ? 1 : 0;
				ope->result.l = (ope->result.l << 1) | Chipset.Cpu.SR.C;
				break;
		}
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.dcycles += 2; 
	return BUS_OK;
}

BYTE OP_ror(OP *ope) {
	ope->eas.data.b[0] &= 0x3F;
	Chipset.Cpu.SR.C = 0;
	Chipset.Cpu.SR.V = 0;
	ope->result.l = ope->ead.data.l;	
	while (ope->eas.data.b[0]--) {
		switch (ope->ead.isize) {
			case 1:
				Chipset.Cpu.SR.C = (ope->result.b[0] & 0x01) ? 1 : 0;
				ope->result.b[0] = (ope->result.b[0] >> 1) | (Chipset.Cpu.SR.C ? 0x80 : 0x00);
				break;
			case 2:
				Chipset.Cpu.SR.C = (ope->result.w[0] & 0x0001) ? 1 : 0;
				ope->result.w[0] = (ope->result.w[0] >> 1) | (Chipset.Cpu.SR.C ? 0x8000 : 0x0000);
				break;
			default:
				Chipset.Cpu.SR.C = (ope->result.l & 0x00000001) ? 1 : 0;
				ope->result.l = (ope->result.l >> 1) | (Chipset.Cpu.SR.C ? 0x80000000 : 0x00000000);
				break;
		}
		Chipset.dcycles += 2; 
	}	
	switch (ope->ead.isize) {
		case 1:
			Chipset.Cpu.SR.Z = RZB;
			Chipset.Cpu.SR.N = RB;
			break;
		case 2:
			Chipset.Cpu.SR.Z = RZW;
			Chipset.Cpu.SR.N = RW;
			break;
		default:
			Chipset.Cpu.SR.Z = RZL;
			Chipset.Cpu.SR.N = RL;
			break;
	}
	Chipset.dcycles += 2; 
	return BUS_OK;
}
// ASL Ea
BYTE OP_aslm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x8000) ? 1 : 0;
	ope->result.w[0] = ope->eas.data.w[0] << 1;
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = (Chipset.Cpu.SR.C != Chipset.Cpu.SR.N);
	Chipset.dcycles += 4;
	return BUS_OK;
}
// ASR Ea
BYTE OP_asrm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x0001) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] >> 1) | (ope->eas.data.w[0] & 0x8000);
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// LSL Ea
BYTE OP_lslm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x8000) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] << 1);
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// LSR Ea
BYTE OP_lsrm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x0001) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] >> 1);
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// ROXL Ea
BYTE OP_roxlm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x8000) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] << 1) | (Chipset.Cpu.SR.X);
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// ROXR Ea
BYTE OP_roxrm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x0001) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] >> 1) | ((Chipset.Cpu.SR.X) ? 0x8000 : 0x0000);
	Chipset.Cpu.SR.X = Chipset.Cpu.SR.C;
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// ROL Ea
BYTE OP_rolm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x8000) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] << 1) | (Chipset.Cpu.SR.C);
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
// ROR Ea
BYTE OP_rorm(OP *ope) {
	Chipset.Cpu.SR.C = (ope->eas.data.w[0] & 0x0001) ? 1 : 0;
	ope->result.w[0] = (ope->eas.data.w[0] >> 1) | ((Chipset.Cpu.SR.C) ? 0x8000 : 0x0000);
	Chipset.Cpu.SR.Z = RZW;
	Chipset.Cpu.SR.N = RW;
	Chipset.Cpu.SR.V = 0;
	Chipset.dcycles += 4;
	return BUS_OK;
}
