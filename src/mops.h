/*
 *   Mops.h
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   Bus commands
//

#define BUS_OK 0x00
#define BUS_ERROR 0x01
#define ADDRESS_ERROR 0x02

//
// SSP and USP push, pop, inc, dec
//

#define DEC4SP { if (Chipset.Cpu.SR.S) Chipset.Cpu.A[8].l -= 4; else Chipset.Cpu.A[7].l -= 4; }
#define DEC2SP { if (Chipset.Cpu.SR.S) Chipset.Cpu.A[8].l -= 2; else Chipset.Cpu.A[7].l -= 2; }
#define INC4SP { if (Chipset.Cpu.SR.S) Chipset.Cpu.A[8].l += 4; else Chipset.Cpu.A[7].l += 4; }
#define INC2SP { if (Chipset.Cpu.SR.S) Chipset.Cpu.A[8].l += 2; else Chipset.Cpu.A[7].l += 2; }
#define PUSHW(data) WriteMEM((BYTE *)& (data), (Chipset.Cpu.SR.S) ? Chipset.Cpu.A[8].l : Chipset.Cpu.A[7].l, 2)
#define PUSHL(data) WriteMEM((BYTE *)& (data), (Chipset.Cpu.SR.S) ? Chipset.Cpu.A[8].l : Chipset.Cpu.A[7].l, 4)
#define POPW(data) ReadMEM((BYTE *)& (data), (Chipset.Cpu.SR.S) ? Chipset.Cpu.A[8].l : Chipset.Cpu.A[7].l, 2)
#define POPL(data) ReadMEM((BYTE *)& (data), (Chipset.Cpu.SR.S) ? Chipset.Cpu.A[8].l : Chipset.Cpu.A[7].l, 4)

//################
//#
//#    Low level subroutines
//#
//################

//
// better mem function as we move only from from 1 to 8 bytes at once without any alignment!
//
static __forceinline VOID memcopy(BYTE *d, BYTE *s, UINT count)
{
	while (count--)
		*d++ = *s++;
}

static __forceinline VOID memzero(BYTE *a, UINT count)
{
	while (count --)
		*a++ = 0x00;
}

static __forceinline VOID memfill(BYTE *a, UINT count)
{
	while (count --)
		*a++ = 0xFF;
}
