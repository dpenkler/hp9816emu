/*
 *	 9816.h
 *       Header file for mc 68000 processor and chipsets for 9816 system
 *	 Copyright 2010-2011 Olivier De Smet
 *                 2021      Dave Penkler
 */

#define M68000									// for pure 68000
// #define M68010								// for 68010 (need to uncomment some stuff elsewhere)

//
// CPU state
//
typedef enum CPU_STATE {
  EXCEPTION,
  NORMAL,
  HALT
} CPU_STATE;

//
// struct for mem acces as byte, word, double word
//
typedef union MEM {
  signed int sl;
  unsigned int l;
  signed short sw[2];
  unsigned short w[2];
  signed char sb[4];
  unsigned char b[4];
} MEM;

//
// extension word data struct for extended addressing mode
//
typedef union EXTW {
  WORD w;
  struct {
    BYTE d8  : 8;
    BYTE fil : 3;
    BYTE wl  : 1;
    BYTE reg : 3;
    BYTE da  : 1;
  };
} EXTW;

//
// MC68000 data struct
//
typedef struct {
  CPU_STATE State;	// cpu state
  DWORD	PC;		// program counter
  MEM	D[8];		// data registers
  MEM	A[9];		// address registers + SSP
  union {
    WORD sr;		// status register as a whole
    BYTE r[2];		// bytes of the sr
    struct {		// bits of the sr
      WORD C   : 1;
      WORD V   : 1;
      WORD Z   : 1;
      WORD N   : 1;
      WORD X   : 1;
      WORD FIL : 3;
      WORD MASK: 3;
      WORD FIL1: 2;
      WORD S   : 1;
      WORD FIL2: 1;
      WORD T   : 1;
    };
  } SR;
  WORD	countOp;	// counter for # of opcodes (for profiling)
  DWORD	lastMEM;	// last address of mem access for exceptions and stack frames
  WORD	lastRW;		// last mem acces as read or write
  BYTE	lastVector;	// last exception vector
  BYTE	I210;		// interrupt input
  BYTE	reset;		// output reset line
  //	DWORD		VBR;				// for 68010
  //	BYTE		SFC;				// for 68010
  //	BYTE		DFC;				// for 68010
} MC68000;

//
// keyboard data struct
//
typedef struct {
  WORD	stKeyb;		// state of 8041 at startup 
  WORD	stKeybrtn;	// return state of 8041 for datain

  BYTE	kc_buffer[256];	// circular buffer for transmitting data from keybord to system
  BYTE	lo_b;		// low mark of this buffer
  BYTE	hi_b;		// high mark of this buffer
  BYTE	ram[64];	// internal ram of 8041 microcontroller
  BYTE	int68000;	// interrupt level wanted ?
  BYTE	intmask;	// mask for 8041 interrupt to 68000
  BYTE	int8041;	// interrupt the 8041
  BYTE	status;		// b2 : 0:RESET key : not done
  DWORD	status_cycles;	// when MC68000 polled the status byte + 1 ms (to avoid race)
  BYTE	send_wait;	// asking for an int, wait the read dataout for next action
  BYTE	command;	// current 8041 command
  BYTE	dataout;	// current 8041 dataout for 68000
  BYTE	datain;		// current 68000 datain for 8041
  BYTE	shift;		// shift key state
  BYTE	ctrl;		// ctrl key state
  BYTE	alt;		// alt key state
  BYTE	altgr;		// altgr key state (for french keymap only)
  BYTE	forceshift;	// change shift state for keycode conversion
  SWORD	knob;		// knob counter (signed)
  BOOL	stdatain;	// strobe for datain
  DWORD	cycles;		// cycles in 1MHz base time
  BYTE	keymap;		// us or fr keymap
} KEYBOARD;

//
// display structure data for 9836A and 9816A
//
typedef struct {
  BYTE	graph_on;		// graph plane visible ?
  WORD	a_xmin;			// refresh rectangle for alpha
  WORD	a_xmax;
  WORD	a_ymin;
  WORD	a_ymax; 
  WORD	g_xmin;			// refresh rectangle for graph
  WORD	g_xmax;
  WORD	g_ymin;
  WORD	g_ymax; 
  WORD	alpha_width;		// alpha width in pc pixels
  WORD	alpha_height;		// alpha height in native pixel
  WORD	alpha_char_w;		// width of a char cell in pc pixels
  WORD	alpha_char_h;		// heigh of a char cell in native pixels
  WORD	graph_width;		// graph width in pc pixels
  WORD	graph_height;		// graph height in native pixels
  WORD	graph_bytes;		// size of the graph memory
  WORD	graph_visible;		// address of last visible graph byte
  ULONG default_pixel;          // default foreground pixel value
  DWORD	cycles;			// cycle counter for vsync
  BYTE	whole;			// not used
  BYTE	reg;			// adressed mc6845 reg
  BYTE	regs[16];		// mc6845 regs
  BYTE	cursor_t;		// top of cursor
  BYTE	cursor_b;		// bottom of cursor
  BYTE	cursor_h;		// heigh of cursor
  BYTE	cursor_blink;		// 0xFF:off, 0x20:1/32, 0x10:1/16, 0x00:on
  WORD	cursor_new;		// new cursor address
  WORD	cursor;			// actual cursor address
  WORD	start;			// address of display start for alpha ram
  WORD	end;			// address of display end for alpha ram
  BYTE	cnt;			// vsync counter
  BYTE	cursor_last;		// last cursor state ...
  BYTE	alpha[4096];		// 4 KB of alpha dual port mem max 
  BYTE	graph[32768];		// 32 KB of graph dual port mem max
} DISPLAY16;

//
// common display structure 
//
typedef struct {
  BYTE	nothing;		// nothing actually 
} DISPLAY;


//
// internal HPIB controller with TI9914A
//
typedef struct {
  BYTE	d_out[0x200];		// circular buffer of data out of controler (back to computer)
  BYTE	c_out[0x200];		// circular buffer of control out of controler (back to computer)
  WORD	h_out_hi;		// hi mark				
  WORD	h_out_lo;		// lo mark
  BYTE	h_dmaen;		// dma enable for hpib ? (not unsed)
  BYTE	h_controller;		// hpib controller is in control ?
  BYTE	h_sysctl;		// hpib is system controller by default ?
  BYTE	h_int;			// hpib interrupt requested

  BYTE	a_swrst;		// ti9914A internal state for swrst
  BYTE	a_dacr;			// ti9914A internal state for dacr
  BYTE	a_hdfa;			// ti9914A internal state for hdfa
  BYTE	a_hdfe;			// ti9914A internal state for hdfe
  BYTE	a_fget;			// ti9914A internal state for fget
  BYTE	a_rtl;			// ti9914A internal state for rtl
  BYTE	a_lon;			// ti9914A internal state for lon
  BYTE	a_ton;			// ti9914A internal state for ton
  BYTE	a_rpp;			// ti9914A internal state for rpp
  BYTE	a_sic;			// ti9914A internal state for sic
  BYTE	a_sre;			// ti9914A internal state for sre
  BYTE	a_dai;			// ti9914A internal state for diseable all interrupt
  BYTE	a_stdl;			// ti9914A internal state for stdl
  BYTE	a_shdw;			// ti9914A internal state for shdw
  BYTE	a_vstdl;		// ti9914A internal state for vstdl
  BYTE	a_rsv2;			// ti9914A internal state for rsv2

  BYTE	s_eoi;			// 9914A send oei with next byte
  union	{                       // ti9914A status 0 byte
    BYTE status0;
    struct {
      WORD mac	:1;		// ti9914A status 0 bits
      WORD rlc	:1;
      WORD spas	:1;
      WORD end	:1;	
      WORD bo	:1;
      WORD bi	:1;
      WORD int1	:1;
      WORD int0	:1;
    };
  };
  union	{
    BYTE status1;
    struct	{
      WORD	ifc	:1;
      WORD	srq	:1;
      WORD	ma	:1;
      WORD	dcas	:1;
      WORD	apt	:1;
      WORD	unc	:1;
      WORD	err	:1;
      WORD	get	:1;
    };
  };
  union	{
    BYTE statusad;
    struct	{
      WORD	ulpa	:1;
      WORD	tads	:1;
      WORD	lads	:1;
      WORD	tpas	:1;
      WORD	lpas	:1;
      WORD	atn	:1;
      WORD	llo	:1;
      WORD	rem	:1;
    };
  };
  union		{
    BYTE	statusbus;
    struct	{
      WORD	l_ren	:1;
      WORD	l_ifc	:1;
      WORD	l_srq	:1;
      WORD	l_eoi	:1;
      WORD	l_nrfd	:1;
      WORD	l_ndac	:1;
      WORD	l_dav	:1;
      WORD	l_atn	:1;
    };
  };
  BYTE	data_bus;	// data on bus for 3 wires handshake
  BYTE	data_in;	// data read after handshake
  BYTE	data_in_read;	// data_in read, can load the next
  BYTE	intmask0;	// ti9914A interrupt mask 0
  BYTE	intmask1;	// ti9914A interrupt mask 1
  BYTE	aux_cmd;	// ti9914A auxilliary command
  BYTE	address;	// ti9914A hpib address
  BYTE	ser_poll;	// ti9914A serial poll byte
  BYTE	par_poll;	// ti9914A parallel poll byte
  BYTE	par_poll_resp;	// par poll response
  BYTE	data_out;	// byte to emit
  BYTE	data_out_loaded;// byte to emit loaded
  BYTE	gts;		// ti9914A standby, ready for a byte ...
} HPIB;

//
// HPIB SS80 disk controller
//
typedef struct {
  BYTE	hpibaddr;		// hpib address off controller
  BYTE	hc[0x400];		// circular buffer of commands
  BYTE	hc_t[0x400];		// circular buffer of delay for transmission
  WORD	hc_hi;			// hi mark
  WORD	hc_lo;			// lo mark
  WORD	hd[0x400];		// circular buffer of data in & eoi
  BYTE	hd_t[0x400];		// circular buffer of delay for transmission
  WORD	hd_hi;			// hi mark
  WORD	hd_lo;			// lo mark
  BYTE	ppol_e;			// parallel poll enabled ?
  BOOL	talk;			// MTA received ?
  BOOL	listen;			// MLA received ?
  BOOL	untalk;			// previous command was UNTALK ?
  BYTE	c;			// command byte
  DWORD	dcount;			// for data count
  DWORD	dindex;
  WORD	count;			// for various count
  WORD	word;
  DWORD	dword;			// for set address and set length
  BYTE	data[512];		// data to send (for read and write too) ... :)
	
  WORD	stss80;			// state

  BYTE	unit;			// current addressed unit	
  BYTE	volume;			// current addressed volume
  WORD	addressh[16];		// current address HIGH
  DWORD	address[16];		// current address	(normally 6 bytes)
  DWORD	length[16];		// current length of transaction
  union {
    BYTE status[8];		// mask status bit errors
    struct {
      WORD address_bounds	:1;
      WORD module_addressing	:1;
      WORD illegal_opcode	:1;
      WORD filler		:2;
      WORD channel_parity_error	:1;
      WORD filler_2		:2; // end of byte 1
      WORD filler_3		:3;
      WORD message_length	:1;
      WORD filler_4		:1;
      WORD message_sequence	:1;
      WORD illegal_parameter	:1;
      WORD parameter_bounds	:1; // end of byte 2
      WORD filler_5		:1; // non maskable byte 3
      WORD unit_fault	        :1;
      WORD filler_6		:2;
      WORD controller_fault	:1;
      WORD filler_7		:1;
      WORD cross_unit		:1;
      WORD filler_8		:1; // end of byte 3
      WORD re_transmit		:1; // non maskable byte 4
      WORD power_fail		:1;
      WORD filler_9		:5;
      WORD diagnostic_result	:1; // end of byte 4
      WORD filler_a		:2;
      WORD no_data_found	:1;
      WORD write_protect	:1;
      WORD not_ready		:1;
      WORD no_spare_available	:1;
      WORD uninitialized_media	:1;
      WORD filler_b		:1; // end of byte 5
      WORD filler_c		:3;
      WORD end_of_volume	:1;
      WORD end_of_file		:1;
      WORD filler_d		:1;
      WORD unrecoverable_data	:1;
      WORD unrecoverable_data_ovf:1; // end of byte 6
      WORD auto_sparing_invoked	:1;
      WORD filler_e		:2;
      WORD latency_induced	:1;
      WORD media_wear		:1;
      WORD filler_f		:3; // end of byte 7
      WORD filler_g		:4;
      WORD recoverable_data	:1;
      WORD filler_h		:1;
      WORD recoverable_data_ovf	:1;
      WORD filler_i		:1;
    };
  } mask[16];				// for all units
  BYTE	rwvd;				// read, write, verify or data execute
  union {
    BYTE status[8];			// status error
    struct {
      WORD address_bounds			:1;
      WORD module_addressing		:1;
      WORD illegal_opcode			:1;
      WORD filler						:2;
      WORD channel_parity_error	:1;
      WORD filler_1				:2;	// end of byte 1
      WORD filler_2						:3;
      WORD message_length			:1;
      WORD filler_3		:1;
      WORD message_sequence	:1;
      WORD illegal_parameter	:1;
      WORD parameter_bounds	:1;	// end of byte 2
      WORD filler_4		:1;
      WORD unit_fault		:1;
      WORD filler_5		:2;
      WORD controller_fault	:1;
      WORD filler_6							:1;
      WORD cross_unit		:1;
      WORD filler_7		:1;	// end of byte 3
      WORD re_transmit		:1;
      WORD power_fail		:1;
      WORD filler_8		:5;
      WORD diagnostic_result	:1;	// end of byte 4
      WORD filler_9		:2;
      WORD no_data_found	:1;
      WORD write_protect	:1;
      WORD not_ready		:1;
      WORD no_spare_available	:1;
      WORD uninitialized_media	:1;
      WORD filler_a		:1;	// end of byte 5
      WORD filler_b		:3;
      WORD end_of_volume	:1;
      WORD end_of_file		:1;
      WORD filler_c		:1;
      WORD unrecoverable_data	:1;
      WORD unrecoverable_data_ovf:1;	// end of byte 6
      WORD auto_sparing_invoked	:1;
      WORD filler_d		:2;
      WORD latency_induced	:1;
      WORD media_wear		:1;
      WORD filler_e		:3;	// end of byte 7
      WORD filler_f		:4;
      WORD recoverable_data	:1;
      WORD filler_g		:1;
      WORD recoverable_data_ovf	:1;
      WORD filler_h		:1;
    };
  } err[16];				// for all units
  BYTE	qstat[16];			// for all units
  BYTE	type[2];			// kind of disk and unit ...
  BYTE	ftype[2];			// kind of disk and unit for format
  INT   hdisk[2];			// handle for disk based disk image
  WORD	ncylinders[2];			// number of usable cylinders
  BYTE	nsectors[2];			// number of sectors per cylinder
  BYTE	nheads[2];			// number of heads
  WORD	nbsector[2];			// bytes per sector
  DWORD	totalsectors[2];		// total number of sectors
  // BYTE		motor[4];	// motor on ?
  BYTE	head[2];			// current head
  WORD	cylinder[2];			// current cylinder
  BYTE	sector[2];			// current sector
  // BYTE		rw[4];		// read or write op ?
  // BYTE		step[4];	// seek in or out ?
  DWORD	addr[2];			// current address in data
  LPBYTE disk[2];			// pointer to data
  BYTE	new_medium[2];			// when a new medium is inserted
  BYTE	lifname[2][8];
  TCHAR	name[2][MAX_PATH];		// 2 units max
} HPSS80;

//
// Amigo protocol HPIB controller
//
typedef struct {
  DWORD	addr[2];	// current address in data
  LPBYTE disk[2];	// pointer to data
  BYTE	ctype;		// kind of controler (0:9121 1:9895 2:9133)

  BYTE	hpibaddr;	// hpib address of controller
  WORD	st9121;		// state of hp9121 controller
  BOOL	talk;		// MTA received ?
  BOOL	listen;		// MLA received ?
  BOOL	untalk;		// previous command was UNTALK ?
  BYTE	s1[2];		// status1
  BYTE	s2[2];		// status2
  BYTE	dsj;		// DSJ of controler
  BYTE	head[2];	// current head
  WORD	cylinder[2];	// current cylinder
  BYTE	sector[2];	// current track
  BYTE	unit;		// last addressed unit
  BYTE	type[2];	// kind of unit
  BYTE	config_heads;	// config of unit (head, cylinder, sect/cylinder)
  WORD	config_cylinders;
  WORD	config_sectors;
  BYTE	hc[0x200];	// stack of commands
  BYTE	hc_t[0x200];	
  WORD	hc_hi;		// hi mark
  WORD	hc_lo;		// lo mark
  WORD	hd[0x200];	// circular buffer of data in
  BYTE	hd_t[0x200];	// circular buffer of delay
  WORD	hd_hi;		// hi mark
  WORD	hd_lo;		// lo mark
  BYTE	unbuffered;	// for buffered, unbuffered read/write
	
  BYTE	c;		// current command
  WORD	d;		// counter
  BYTE	message[4];	// message status or address

  BOOL	ppol_e;		// parallel poll enabled

  BYTE	lifname[2][8];	// name of lif volume in drive
  TCHAR	name[2][MAX_PATH]; // 2 units max
} HP9121;

//
// HPIB HP2225 printer like
//
typedef struct {
  BYTE	hpibaddr;	// hpib address
  //	WORD	st2225;	// state of hp2225 controller, not used
  BOOL	talk;		// MTA received ?
  BOOL	listen;		// MLA received ?
  BOOL	untalk;		// previous command was UNTALK ?
  BYTE	hc[0x200];	// circular buffer of commands
  BYTE	hc_t[0x200];	// circular buffer of delay
  WORD	hc_hi;		// hi mark
  WORD	hc_lo;		// lo mark
  WORD	hd[0x200];	// circular buffer of data in
  BYTE	hd_t[0x200];	// circular buffer of delay
  WORD	hd_hi;		// hi mark
  WORD	hd_lo;		// lo mark
	
  BOOL	ppol_e;		// parallel poll enabled
	
  INT   hfile;		// handle for file
  WORD	fn;		// number for file name
  TCHAR	name[MAX_PATH];	// file name
} HP2225;

//
// HP98635A floating point card based on National 16081
//
typedef struct {
  union {
    double l[4];		// 4 double regs
    float  f[8];		// 8 float regs
    DWORD  d[8];		// 8 longs for copying
    BYTE   b[32];		// 32 bytes for copying
  };
  union {
    DWORD fstatus;		// status of 16081
    struct {
      WORD s_tt  : 3;
      WORD s_un  : 1;
      WORD s_uf  : 1;
      WORD s_in  : 1;
      WORD s_if  : 1;
      WORD s_rm  : 2;
      WORD s_swf : 7;
      WORD s_fil: 16;
    } fs;
  };
  BYTE status;			// status of card
} HP98635;

//
// HP98626 internal serial card for 9816
//
typedef struct {
  BYTE	control;		// control byte
  BYTE	status;			// status byte
  BYTE	data_in;		// data in
  BYTE	data_out;		// data out
  BOOL	inte;			// want an int from 68901 ?
  BYTE	regs[8];		// regs of mc68..
  BYTE	fifo_in[64];		// circular buffer receive fifo
  BYTE	fifo_in_t;		// fifo in top
  BYTE	fifo_in_b;		// fifo in bottom
} HP98626;

//
// whole chipmunk system (with all configs)
//
typedef struct {
  WORD	  nPosX;	// window X pos
  WORD	  nPosY;	// window Y pos
  UINT    type;		// computer type (16, 26, 35, 36, 37)

  WORD	  RomVer;	// rom version
  LPBYTE  Rom;		// pointer on system rom  (ie 0x000000 - 0x00FFFF)
  DWORD	  RomSize;	// rom size ...
  LPBYTE  Ram;		// pointer to ram	  (ie 0xC00000 - 0xFFFFFF)
  DWORD	  RamStart;	// address of start of ram
  DWORD	  RamSize;	// ram size
  MC68000 Cpu;		// CPU
  long	  cycles;	// oscillator cycles
  WORD	  dcycles;	// last instruction duration
  long	  ccycles;	// duration in cycles from last µs checkpoint
  BYTE	  I210;		// interrupt level pending

  BYTE	  keeptime;	// keep real time

  DWORD	  annun;	// current display annunciators
  DWORD	  pannun;	// previous display annunciators

  BYTE	  switch1;	// motherboard switches bank 1
  BYTE	  switch2;	// motherboard switches bank 2

  DISPLAY   Display;	// common display subsystem
  DISPLAY16 Display16;	// 9816, 9836 display subsystem

  KEYBOARD  Keyboard;	// one keyboard

  HPIB	 Hpib;		// internal system controller HPIB
  BYTE	 Hpib70x;	// type for 70x unit (9121, 9895,9134,9122)
  BYTE	 Hpib71x;	// type for 71x unit here only hp 2225 printer if not null
  BYTE	 Hpib72x;	// type for 72x unit (9121, 9895,9134,9122)
  BYTE	 Hpib73x;	// type for 73x unit (7908, 7911, 7912)
  BYTE	 Hpib74x;	// type for 74x unit (7908, 7911, 7912)

  HPSS80 Hp9122;	// HPIB external floppy 9122 address 0
  HP9121 Hp9121;	// HPIB external floppy 9121 address 2
  HPSS80 Hp7908_0;	// HPIB external hard disk address 3
  HPSS80 Hp7908_1;	// HPIB external hard disk address 4

  HP2225 Hp2225;	// HPIB external printer address 1

  BYTE	  Hp98635;	// floating point card present ?
  HP98635 Nat;		// national 16081 floating point card

  HP98626 Serial;	// internal serial for HP9816 (dummy one, only regs)

} SYSTEM;
