/*
 *   Hp-98635.c
 *
 *   Copyright 2010-2011 Olivier De Smet
 */

//
//   national 16081 floating point card 
// TT 0 no exception
//    1 underflow if UN set
//    2 overflow
//    3 divide by zero
//    4 illegal instruction (can't happen)
//    5 invalid operand if 0/0 or ieee operand used : not used by pascal
//    6 inexact result if IN set

#include "common.h"
#include "hp9816emu.h"
#include "kml.h"
#include "mops.h"					// I/O definitions


#include <math.h>
#include <float.h>
#include <fenv.h>

//#define DEBUG_98635					// debug flag

#if defined DEBUG_98635
static int bDebugOn = 1;
static	TCHAR buffer[256];
static	int k;
#define OutputDebugString(X) fprintf(stderr,X)
#endif

//################
//#
//#    Low level subroutines
//#
//################

static VOID addl(BYTE s, BYTE d) {
	Chipset.Nat.l[d] = Chipset.Nat.l[d] + Chipset.Nat.l[s];
	__asm ("fwait");		// to catch underflow
	#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
		k = snprintf(buffer, 256, _T("%06X: HP98635 addl f%d f%d = %g\n"), Chipset.Cpu.PC, s*2, d*2, Chipset.Nat.l[d]);
		OutputDebugString(buffer); buffer[0] = 0x00;
	}
	#endif
}

static VOID subl(BYTE s, BYTE d) {
	Chipset.Nat.l[d] = Chipset.Nat.l[d] - Chipset.Nat.l[s];
	__asm ("fwait");		// to catch underfow
	#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
		k = snprintf(buffer, 256, _T("%06X: HP98635 subl f%d f%d = %g\n"), Chipset.Cpu.PC, s*2, d*2, Chipset.Nat.l[d]);
		OutputDebugString(buffer); buffer[0] = 0x00;
	}
	#endif
}

static VOID mull(BYTE s, BYTE d) {
	Chipset.Nat.l[d] = Chipset.Nat.l[d] * Chipset.Nat.l[s];
	__asm ("fwait");		// to catch underfow
	#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
		k = snprintf(buffer, 256, _T("%06X: HP98635 mull f%d f%d = %g\n"), Chipset.Cpu.PC, s*2, d*2, Chipset.Nat.l[d]);
		OutputDebugString(buffer); buffer[0] = 0x00;
	}
	#endif
}

static VOID divl(BYTE s, BYTE d) {
  Chipset.Nat.l[d] = Chipset.Nat.l[d] / Chipset.Nat.l[s];
  __asm ("fwait");		// to catch underfow
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 divl f%d f%d = %g\n"), Chipset.Cpu.PC, s*2, d*2, Chipset.Nat.l[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}

static VOID negl(BYTE s, BYTE d) {
  Chipset.Nat.l[d] = - Chipset.Nat.l[s];
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 negl f%d f%d = %g\n"), Chipset.Cpu.PC, s*2, d*2, Chipset.Nat.l[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}
static VOID absl(BYTE s, BYTE d) {
  Chipset.Nat.l[d] = fabs(Chipset.Nat.l[s]);
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 absl f%d f%d = %g\n"), Chipset.Cpu.PC, s*2, d*2, Chipset.Nat.l[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}

static VOID addf(BYTE s, BYTE d) {
  Chipset.Nat.f[d] = Chipset.Nat.f[d] + Chipset.Nat.f[s];
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 addf f%d f%d = %g\n"), Chipset.Cpu.PC, s, d, (double) Chipset.Nat.f[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}

static VOID subf(BYTE s, BYTE d) {
  Chipset.Nat.f[d] = Chipset.Nat.f[d] - Chipset.Nat.f[s];
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 subf f%d f%d = %g\n"), Chipset.Cpu.PC, s, d, (double) Chipset.Nat.f[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}

static VOID mulf(BYTE s, BYTE d) {
  Chipset.Nat.f[d] = Chipset.Nat.f[d] * Chipset.Nat.f[s];
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 mulf f%d f%d = %g\n"), Chipset.Cpu.PC, s, d, (double) Chipset.Nat.f[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}
static VOID divf(BYTE s, BYTE d) {
  Chipset.Nat.f[d] = Chipset.Nat.f[d] / Chipset.Nat.f[s];
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 divf f%d f%d = %g\n"), Chipset.Cpu.PC, s, d, (double) Chipset.Nat.f[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}

static VOID negf(BYTE s, BYTE d) {
  Chipset.Nat.f[d] = - Chipset.Nat.f[s];
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 negf f%d f%d = %g\n"), Chipset.Cpu.PC, s, d, (double) Chipset.Nat.f[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif

}
static VOID absf(BYTE s, BYTE d) {
  Chipset.Nat.f[d] = fabsf(Chipset.Nat.f[s]);
  __asm("fwait");
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 absf f%d f%d = %g\n"), Chipset.Cpu.PC, s, d, (double) Chipset.Nat.f[d]);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
}

//################
//#
//#    Public functions
//#
//################

typedef union DWBF {
  BYTE b[4];
  unsigned int d;
  float f;
} DWBF;

//
// write in National 16081 IO space
// 
BYTE Write_98635(BYTE *a, WORD d, BYTE s) {
#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 write %04X(%d)\n"), Chipset.Cpu.PC, d, s);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
  if (d & 0x4000) {		// ops
    DWBF dwbf;
    WORD dd = d & 0x0FFE;

    if (s != 4)   fprintf(stderr,"98635 write access not in dword !!");

    if (dd < 0x04E0) {
      return BUS_ERROR;
    } else if (dd < 0x0500) {		// movf_m_fx
      BYTE dest = (0x4FC - dd) >> 2;
      dwbf.b[3] = *(--a);
      dwbf.b[2] = *(--a);
      dwbf.b[1] = *(--a);
      dwbf.b[0] = *(--a);
      Chipset.Nat.d[dest] = dwbf.d;
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movf f%d <- %g (%g)\n"), Chipset.Cpu.PC, dest, (double) Chipset.Nat.f[dest], Chipset.Nat.l[dest>>1]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
      return BUS_OK;
    } else if (dd < 0x0520) {		// movif_m_fx
      BYTE dest = (0x51C - dd) >> 2;
      dwbf.b[3] = *(--a);
      dwbf.b[2] = *(--a);
      dwbf.b[1] = *(--a);
      dwbf.b[0] = *(--a);
      Chipset.Nat.f[dest] = (float) (dwbf.d);
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movif f%d <- %g\n"), Chipset.Cpu.PC, dest, (double) Chipset.Nat.f[dest]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
      return BUS_OK;
    } else if (dd < 0x0530) {		// movil_m_fx
      BYTE dest = (0x52C - dd) >> 2;
      dwbf.b[3] = *(--a);
      dwbf.b[2] = *(--a);
      dwbf.b[1] = *(--a);
      dwbf.b[0] = *(--a);
      Chipset.Nat.l[dest] = (double) (dwbf.d);
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movil f%d <- %g\n"), Chipset.Cpu.PC, dest*2, Chipset.Nat.l[dest]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
      return BUS_OK;
    } else if (dd < 0x0540) {		// movfl_m_fx
      BYTE dest = (0x53C - dd) >> 2;
      dwbf.b[3] = *(--a);
      dwbf.b[2] = *(--a);
      dwbf.b[1] = *(--a);
      dwbf.b[0] = *(--a);
      Chipset.Nat.l[dest] = (double) (dwbf.f);
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movfl f%d <- %g\n"), Chipset.Cpu.PC, dest*2, Chipset.Nat.l[dest]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
      return BUS_OK;
    } else if (dd == 0x0540) {		// LFSR_m_m	write fpu status register 
      dwbf.b[3] = *(--a);
      dwbf.b[2] = *(--a);
      dwbf.b[1] = *(--a);
      dwbf.b[0] = *(--a);
      Chipset.Nat.fstatus = dwbf.d;

#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 write fstatus = %04X\n"), Chipset.Cpu.PC, Chipset.Nat.fstatus);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
      return BUS_OK;
    } 
    return BUS_ERROR;
  } else {
    BYTE data;

    while (s) {
      switch (d) {
      case 0x00:
	a--;
	break;
      case 0x01:
	data = *(--a);
	if (data == 0x01) {		// reset card
#if defined DEBUG_98635
	  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	    k = snprintf(buffer, 256, _T("%06X: HP98635 reset card\n"), Chipset.Cpu.PC);
	    OutputDebugString(buffer); buffer[0] = 0x00;
	  }
#endif
	} else if (data == 0x00) {	// idle loop mode, reset to leave
#if defined DEBUG_98635
	  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	    k = snprintf(buffer, 256, _T("%06X: HP98635 idle card\n"), Chipset.Cpu.PC);
	    OutputDebugString(buffer); buffer[0] = 0x00;
	  }
#endif
	}
	break;
      case 0x10:					// bogus read zone ... writing allowed ?
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x14:
      case 0x15:
      case 0x16:
      case 0x17:
      case 0x18:
      case 0x19:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
      case 0x1f:
	a--;
	break;
      case 0x20:
	a--;
	break;
      case 0x21:
	if (*(--a) == 0x00) {		// reset state machine 
	  Chipset.Nat.status = 0;
	  Chipset.Nat.fs.s_tt = 0;
#if defined DEBUG_98635
	  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	    k = snprintf(buffer, 256, _T("%06X: HP98635 reset state machine\n"), Chipset.Cpu.PC);
	    OutputDebugString(buffer); buffer[0] = 0x00;
	  }
#endif
	}
	break;
      default:
	a--;
	break;
      }
      s--;
      d++;
    }
  }
  return BUS_OK;
}

//
// read in National 16081 IO space
//
BYTE Read_98635(BYTE *a, WORD d, BYTE s) {
  fexcept_t fpu;

#if defined DEBUG_98635
  if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
    k = snprintf(buffer, 256, _T("%06X: HP98635 read %04X(%d)\n"), Chipset.Cpu.PC, d, s);
    OutputDebugString(buffer); buffer[0] = 0x00;
  }
#endif
  if (d & 0x4000) {		// ops
    WORD dd = d & 0x0FFE;
    BYTE dl = (BYTE) ((dd & 0x0006) >> 1); 
    BYTE sl = (BYTE) ((dd & 0x0018) >> 3); 
    BYTE df = (BYTE) ((dd & 0x000E) >> 1); 
    BYTE sf = ((BYTE) ((dd & 0x0070) >> 4) + 4) & 0x07; 
    DWBF dwbf;

    if (dd < 0x0580) {
      Chipset.Nat.fstatus &= 0xFFFFFFA8;	// clear status bits
      Chipset.Nat.status = 0x00;
    }
    if (dd < 0x0020) {
      addl(sl, dl);
    } else if (dd < 0x0040) {
      subl(sl, dl);
    } else if (dd < 0x0060) {
      mull(sl, dl);
    } else if (dd < 0x0080) {
      divl(sl, dl);
    } else if (dd < 0x00A0) {
      negl(sl, dl);
    } else if (dd < 0x00C0) {
      absl(sl, dl);
    } else if (dd < 0x0140) {
      addf(sf, df);
    } else if (dd < 0x01C0) {
      subf(sf, df);
    } else if (dd < 0x0240) {
      mulf(sf, df);
    } else if (dd < 0x02C0) {
      divf(sf, df);
    } else if (dd < 0x0340) {
      negf(sf, df);
    } else if (dd < 0x03C0) {
      absf(sf, df);
    } else if (dd < 0x0400) {			// movfl_fx_fy
      BYTE src = (dd & 0x0038) >> 3;
      Chipset.Nat.l[dl] = (double) Chipset.Nat.f[src];
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movfl f%d f%d = %g\n"), Chipset.Cpu.PC, src, dl*2, Chipset.Nat.l[dl]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
    } else if (dd < 0x0440) {			// movlf_fx_fy
      BYTE src = (dd & 0x0030) >> 4;
      Chipset.Nat.f[df] = (float) Chipset.Nat.l[src];
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movlf f%d f%d = %g\n"), Chipset.Cpu.PC, src*2, df, (double) Chipset.Nat.f[df]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
    } else if (dd < 0x0460) {			// movl
      Chipset.Nat.l[dl] = Chipset.Nat.l[sl];
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movl f%d f%d = %g\n"), Chipset.Cpu.PC, sl*2, dl*2, Chipset.Nat.l[dl]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
    } else if (dd < 0x04E0) {			// movf
      Chipset.Nat.d[df] = Chipset.Nat.d[sf];
#if defined DEBUG_98635
      if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	k = snprintf(buffer, 256, _T("%06X: HP98635 movf f%d f%d = %g\n"), Chipset.Cpu.PC, sf, df, (double) Chipset.Nat.f[df]);
	OutputDebugString(buffer); buffer[0] = 0x00;
      }
#endif
    } else if (dd < 0x0550) {
      return BUS_ERROR;
    } else if (dd < 0x0570) {		// movf_fx_m
      if (s == 4) {
	BYTE src = (0x56C - dd) >> 2;
	dwbf.d = Chipset.Nat.d[src];
	*(--a) = dwbf.b[3];
	*(--a) = dwbf.b[2];
	*(--a) = dwbf.b[1];
	*(--a) = dwbf.b[0];
#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	  k = snprintf(buffer, 256, _T("%06X: HP98635 movf f%d = %g (%g)\n"), Chipset.Cpu.PC, src, (double) dwbf.f, Chipset.Nat.l[src>>1]);
	  OutputDebugString(buffer); buffer[0] = 0x00;
	}
#endif
      } else {
	fprintf(stderr,"98635 read access not in dword !!");
	return BUS_ERROR;
      }
    } else if (dd < 0x0580) {		// movlf_fx_m
      if (s == 4) {
	BYTE src = (0x57C - dd) >> 2;
	dwbf.f = (float) Chipset.Nat.l[src];
	*(--a) = dwbf.b[3];
	*(--a) = dwbf.b[2];
	*(--a) = dwbf.b[1];
	*(--a) = dwbf.b[0];
#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	  k = snprintf(buffer, 256, _T("%06X: HP98635 movlf f%d = %g\n"), Chipset.Cpu.PC, src*2, (double) dwbf.f);
	  OutputDebugString(buffer); buffer[0] = 0x00;
	}
#endif
      } else {
	fprintf(stderr,"98635 read access not in dword !!");
	return BUS_ERROR;
      }
    } else if (dd == 0x0580) {		// SFSR_m_m read fpu status register
      if (s == 4) {
	dwbf.d = Chipset.Nat.fstatus;
	*(--a) = dwbf.b[3];
	*(--a) = dwbf.b[2];
	*(--a) = dwbf.b[1];
	*(--a) = dwbf.b[0];
#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	  k = snprintf(buffer, 256, _T("%06X: HP98635 read f status reg %08X\n"), Chipset.Cpu.PC, dwbf.d);
	  OutputDebugString(buffer); buffer[0] = 0x00;
	}
#endif
      } else {
	fprintf(stderr,"98635 read access not in dword !!");
	return BUS_ERROR;
      }
    } else
      return BUS_ERROR;

    fpu = fegetexceptflag(&fpu,FE_ALL_EXCEPT);
    feclearexcept(FE_ALL_EXCEPT);
#if defined DEBUG_98635
    if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
      k = snprintf(buffer, 256, _T("%06X: HP98635 intel fpu status %08X\n"), Chipset.Cpu.PC, fpu);
      OutputDebugString(buffer); buffer[0] = 0x00;
    }
#endif
    if (fpu &  FE_DIVBYZERO) {
      Chipset.Nat.fs.s_tt = 3;
      Chipset.Nat.status |= 0x08;
    } else if (fpu & FE_OVERFLOW) {
      Chipset.Nat.fs.s_tt = 2;
      Chipset.Nat.status |= 0x08;
    } else if (fpu & FE_UNDERFLOW) {
      Chipset.Nat.fs.s_uf = 1;
      if (Chipset.Nat.fs.s_un) {
	Chipset.Nat.fs.s_tt = 1;
	Chipset.Nat.status |= 0x08;
      }
      //		} else if (fpu & _EM_DENORMAL) {
      //			Chipset.Nat.fs.s_tt = 5;
      //			Chipset.Nat.status |= 0x08;
    } else if (fpu & FE_INEXACT) {
      Chipset.Nat.fs.s_if = 1;
      if (Chipset.Nat.fs.s_in) {
	Chipset.Nat.fs.s_tt = 6;
	Chipset.Nat.status |= 0x08;
      }
    }
    return BUS_OK;
  } else {
    while (s) {
      switch (d) {
      case 0x00:
	*(--a) = 0x00;
	break;
      case 0x01:
	*(--a) = 0x0A;		// card id
#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	  k = snprintf(buffer, 256, _T("%06X: HP98635 read card ID 0A\n"), Chipset.Cpu.PC);
	  OutputDebugString(buffer); buffer[0] = 0x00;
	}
#endif
	break;
      case 0x10:				// bogus read
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x14:
      case 0x15:
      case 0x16:
      case 0x17:
      case 0x18:
      case 0x19:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
      case 0x1f:
      case 0x20:
	*(--a) = 0x00;
	break;
      case 0x21:
	*(--a) = (Chipset.Nat.fs.s_tt != 0) ? 8 : 0;
#if defined DEBUG_98635
	if ((Chipset.Cpu.PC > 0xC000) && bDebugOn) {
	  k = snprintf(buffer, 256, _T("%06X: HP98635 read status reg %02X\n"), Chipset.Cpu.PC, *a);
	  OutputDebugString(buffer); buffer[0] = 0x00;
	}
#endif
	break;
      default:
	return BUS_ERROR;
	break;
      }
      s--;
      d++;
    }
    return BUS_OK;
  }
}

VOID Reset_Nat(VOID)
{
}
