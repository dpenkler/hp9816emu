/*
 *   Opcodes.h
 *
 *   Declarations for effective address mode decode and computation
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

typedef enum
{
	EA_NONE,
	EA_CST,			// #cste in opcode
	EA_D,			// Dn
	EA_A,			// An
	EA_IA,			// (An)
	EA_IAI,			// (An)+
	EA_IAD,			// -(An)
	EA_IDA,			// (d16,An)
	EA_IDAX,		// (d8,An,Xn)
	EA_IXW,			// (xxxx).W
	EA_IXL,			// (xxxxxxxx).L
	EA_IDP,			// (d16,PC) as source
	EA_IDPX,		// (d8,PC,Xn) as source
	EA_IMM,			// #IMM as source
	EA_CCR,			// for ,CCR
	EA_SR,			// for ,SR
	EA_USP,			// for MOVE from/to USP		
	EA_MOVEM,		// only for disassembler
	EA_BCC,			// for Bcc
	EA_DBCC,		// for DBcc
	EA_RC			// for movec Register control
} EA_MODE;

typedef struct
{
	EA_MODE imode;			// mode of EA
	BYTE reg;				// reg number
	BYTE isize;				// .BWL of EA
	MEM addr;				// addr of EA
	MEM data;				// value at (addr) of EA
	BYTE calc;				// only for disasm
} EA;

typedef struct tag_op OP;

struct tag_op
{
	EA eas;					// EA source, read or calc (LEA, PEA, JSR)
	EA ead;					// EA destination
	BOOL sd;				// monadic op (read eas, do the op then write at source also) 
	BOOL reas;				// read eas before (after calc)
	BOOL read;				// read ead before 
	BYTE (*op)(OP *);		// OP to do
	BOOL wead;				// write back result to ead ?
	MEM result;				// result to write back
	BYTE vector;			// exception vector if needed
};

