#include <math.h>
#include "mips.h"
#include "opcodes.h"
#include <assert.h>
#include "app_syscall.h"

/*------------------------------------------------------------------------
 *
 *  Instruction exec 
 *
 *------------------------------------------------------------------------
 */
void
Mipc::Dec (unsigned int ins, Bool actual)
{
   MipsInsn i;
   signed int a1, a2;
   unsigned int ar1, ar2, s1, s2, r1, r2, t1, t2;
   LL addr;
   unsigned int val;
   LL value, mask;
   int sa,j;
   Word dummy;

   unsigned int _pc = IF_ID_C._pc;                            // Program counter
   signed int   _decodedSRC1 = 0, _decodedSRC2 = 0;     // Reg fetch output (source values)
   unsigned     _regSRC1 = DEFAULT_REG_VALUE, _regSRC2 = DEFAULT_REG_VALUE;
   Bool         _requireFPArg = FALSE;
   Bool         _requireHi =FALSE, _requireLo = FALSE;
   unsigned     _decodedDST = 0;                    // Decoder output (dest reg no)
   unsigned     _subregOperand = 0;                 // Needed for lwl and lwr
   Bool         _memControl = FALSE;                    // Memory instruction?
   Bool         _writeREG = FALSE, _writeFREG = FALSE;          // WB control
   signed int   _branchOffset = 0;
   Bool         _hiWPort = FALSE, _loWPort = FALSE;             // WB control
   unsigned     _decodedShiftAmt = 0;               // Shift amount
   int          _bd = 0;                            // 1 if the next ins is delay slot
   unsigned int _btgt = 0;                          // branch target
   Bool         _isSyscall = FALSE;                     // 1 if system call
   Bool         _isIllegalOp = FALSE;                   // 1 if illegal opcode
   void (*_opControl)(Mipc*, unsigned) = NULL;
   void (*_memOp)(Mipc*) = NULL;

   i.data = ins;
   _isSubRegOps = FALSE;
  
#define SIGN_EXTEND_BYTE(x)  do { x <<= 24; x >>= 24; } while (0)
#define SIGN_EXTEND_IMM(x)   do { x <<= 16; x >>= 16; } while (0)

   switch (i.reg.op) {
   case 0:
      // SPECIAL (ALU format)
      _decodedSRC1 = _gpr[i.reg.rs];
      _decodedSRC2 = _gpr[i.reg.rt];
      _regSRC1 = i.reg.rs;
      _regSRC2 = i.reg.rt;
      _decodedDST = i.reg.rd;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;

      switch (i.reg.func) {
      case 0x20:			// add
      case 0x21:			// addu
         _opControl = func_add_addu;
	 break;

      case 0x24:			// and
         _opControl = func_and;
	 break;

      case 0x27:			// nor
         _opControl = func_nor;
	 break;

      case 0x25:			// or
         _opControl = func_or;
	 break;

      case 0:			// sll
         _opControl = func_sll;
         _decodedShiftAmt = i.reg.sa;
	 break;

      case 4:			// sllv
         _opControl = func_sllv;
	 break;

      case 0x2a:			// slt
         _opControl = func_slt;
	 break;

      case 0x2b:			// sltu
         _opControl = func_sltu;
	 break;

      case 0x3:			// sra
         _opControl = func_sra;
         _decodedShiftAmt = i.reg.sa;
	 break;

      case 0x7:			// srav
         _opControl = func_srav;
	 break;

      case 0x2:			// srl
         _opControl = func_srl;
         _decodedShiftAmt = i.reg.sa;
	 break;

      case 0x6:			// srlv
         _opControl = func_srlv;
	 break;

      case 0x22:			// sub
      case 0x23:			// subu
	 // no overflow check
         _opControl = func_sub_subu;
	 break;

      case 0x26:			// xor
         _opControl = func_xor;
	 break;

      case 0x1a:			// div
         _opControl = func_div;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x1b:			// divu
         _opControl = func_divu;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x10:			// mfhi
         _opControl = func_mfhi;
         _requireHi = TRUE;
	 break;

      case 0x12:			// mflo
         _opControl = func_mflo;
         _requireLo = TRUE;
	 break;

      case 0x11:			// mthi
         _opControl = func_mthi;
         _hiWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x13:			// mtlo
         _opControl = func_mtlo;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x18:			// mult
         _opControl = func_mult;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0x19:			// multu
         _opControl = func_multu;
         _hiWPort = TRUE;
         _loWPort = TRUE;
         _writeREG = FALSE;
          _writeFREG = FALSE;
	 break;

      case 9:			// jalr
         _opControl = func_jalr;
         _btgt = _decodedSRC1;
         _bd = 1;
         break;

      case 8:			// jr
         _opControl = func_jr;
         _writeREG = FALSE;
         _writeFREG = FALSE;
         _btgt = _decodedSRC1;
         _bd = 1;
	 break;

      case 0xd:			// await/break
         _opControl = func_await_break;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;

      case 0xc:			// syscall
         _opControl = func_syscall;
         _writeREG = FALSE;
         _writeFREG = FALSE;
         _isSyscall = TRUE;
	 break;

      default:
	      _isIllegalOp = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
	 break;
      }
      break;	// ALU format

   case 8:			// addi
   case 9:			// addiu
      // ignore overflow: no exceptions
      _opControl = func_addi_addiu;
      _decodedSRC1 = _gpr[i.imm.rs];
      _regSRC1 = i.imm.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
       _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 0xc:			// andi
      _opControl = func_andi;
      _decodedSRC1 = _gpr[i.imm.rs];
      _regSRC1 = i.imm.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 0xf:			// lui
      _opControl = func_lui;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 0xd:			// ori
      _opControl = func_ori;
      _decodedSRC1 = _gpr[i.imm.rs];
      _regSRC1 = i.imm.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 0xa:			// slti
      _opControl = func_slti;
      _decodedSRC1 = _gpr[i.imm.rs];
      _regSRC1 = i.imm.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 0xb:			// sltiu
      _opControl = func_sltiu;
      _decodedSRC1 = _gpr[i.imm.rs];
      _regSRC1 = i.imm.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 0xe:			// xori
      _opControl = func_xori;
      _decodedSRC1 = _gpr[i.imm.rs];
      _regSRC1 = i.imm.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.imm.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;

   case 4:			// beq
      _opControl = func_beq;
      _decodedSRC1 = _gpr[i.imm.rs];
      _decodedSRC2 = _gpr[i.imm.rt];
      _regSRC1 = i.imm.rs;
      _regSRC2 = i.imm.rt;
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      break;

   case 1:
      // REGIMM
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;

      switch (i.reg.rt) {
      case 1:			// bgez
         _opControl = func_bgez;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
	 break;

      case 0x11:			// bgezal
         _opControl = func_bgezal;
         _decodedDST = 31;
         _writeREG = TRUE;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
	 break;

      case 0x10:			// bltzal
         _opControl = func_bltzal;
         _decodedDST = 31;
         _writeREG = TRUE;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
	 break;

      case 0x0:			// bltz
         _opControl = func_bltz;
         _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
	 break;

      default:
	 _isIllegalOp = TRUE;
	 break;
      }
      break;

   case 7:			// bgtz
      _opControl = func_bgtz;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      break;

   case 6:			// blez
      _opControl = func_blez;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      break;

   case 5:			// bne
      _opControl = func_bne;
      _decodedSRC1 = _gpr[i.reg.rs];
      _decodedSRC2 = _gpr[i.reg.rt];
      _regSRC1 = i.reg.rs;
      _regSRC2 = i.reg.rt;
      _branchOffset = i.imm.imm;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _branchOffset <<= 16; _branchOffset >>= 14; _bd = 1; _btgt = (unsigned)((signed)_pc+_branchOffset+4);
      break;

   case 2:			// j
      _opControl = func_j;
      _branchOffset = i.tgt.tgt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _btgt = ((_pc+4) & 0xf0000000) | (_branchOffset<<2); _bd = 1;
      break;

   case 3:			// jal
      _opControl = func_jal;
      _branchOffset = i.tgt.tgt;
      _decodedDST = 31;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      _btgt = ((_pc+4) & 0xf0000000) | (_branchOffset<<2); _bd = 1;
      break;

   case 0x20:			// lb  
      _opControl = func_lb;
      _memOp = mem_lb;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x24:			// lbu
      _opControl = func_lbu;
      _memOp = mem_lbu;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x21:			// lh
      _opControl = func_lh;
      _memOp = mem_lh;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x25:			// lhu
      _opControl = func_lhu;
      _memOp = mem_lhu;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x22:			// lwl
      _opControl = func_lwl;
      _memOp = mem_lwl;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _subregOperand = _gpr[i.reg.rt];
      _regSRC2 = i.reg.rt;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;

      _isSubRegOps = TRUE;
      break;

   case 0x23:			// lw
      _opControl = func_lw;
      _memOp = mem_lw;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;

      break;

   case 0x26:			// lwr
      _opControl = func_lwr;
      _memOp = mem_lwr;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _subregOperand = _gpr[i.reg.rt];
      _regSRC2 = i.reg.rt;
      _decodedDST = i.reg.rt;
      _writeREG = TRUE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;

      _isSubRegOps = TRUE;
      break;

   case 0x31:			// lwc1
      _opControl = func_lwc1;
      _memOp = mem_lwc1;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = TRUE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;

      break;

   case 0x39:			// swc1
      _opControl = func_swc1;
      _memOp = mem_swc1;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x28:			// sb
      _opControl = func_sb;
      _memOp = mem_sb;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x29:			// sh  store half word
      _opControl = func_sh;
      _memOp = mem_sh;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x2a:			// swl
      _opControl = func_swl;
      _memOp = mem_swl;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x2b:			// sw
      _opControl = func_sw;
      _memOp = mem_sw;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x2e:			// swr
      _opControl = func_swr;
      _memOp = mem_swr;
      _decodedSRC1 = _gpr[i.reg.rs];
      _regSRC1 = i.reg.rs;
      _decodedSRC2 = i.imm.imm;
      _decodedDST = i.reg.rt;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = TRUE;
      break;

   case 0x11:			// floating-point
      // count stats only when it is actual decoding and not decoding for checking dependencies
      if(actual)
         _fpinst++;
      switch (i.freg.fmt) {
      case 4:			// mtc1
         _opControl = func_mtc1;
         _decodedSRC1 = _gpr[i.freg.ft];
         _regSRC1 = i.freg.ft;
         _decodedDST = i.freg.fs;
         _writeREG = FALSE;
         _writeFREG = TRUE;
         _hiWPort = FALSE;
         _loWPort = FALSE;
         _memControl = FALSE;
	 break;

      case 0:			// mfc1
         _opControl = func_mfc1;
         _decodedSRC1 = _fpr[(i.freg.fs)>>1].l[FP_TWIDDLE^((i.freg.fs)&1)];
         _regSRC1 = i.freg.fs;
         _decodedDST = i.freg.ft;
         _writeREG = TRUE;
         _writeFREG = FALSE;
         _hiWPort = FALSE;
         _loWPort = FALSE;
         _memControl = FALSE;
         _requireFPArg = TRUE;
	 break;
      default:
         _isIllegalOp = TRUE;
         _writeREG = FALSE;
         _writeFREG = FALSE;
         _hiWPort = FALSE;
         _loWPort = FALSE;
         _memControl = FALSE;
	 break;
      }
      break;
   default:
      _isIllegalOp = TRUE;
      _writeREG = FALSE;
      _writeFREG = FALSE;
      _hiWPort = FALSE;
      _loWPort = FALSE;
      _memControl = FALSE;
      break;
   }

   IF_ID_C._decodedSRC1 = _decodedSRC1; 
   IF_ID_C._decodedSRC2 = _decodedSRC2;     // Reg fetch output (source values)
   IF_ID_C._regSRC1 = _regSRC1; 
   IF_ID_C._regSRC2 = _regSRC2;
   IF_ID_C._requireFPArg = _requireFPArg;
   IF_ID_C._requireHi = _requireHi;
   IF_ID_C._requireLo = _requireLo;
   IF_ID_C._decodedDST = _decodedDST;                    // Decoder output (dest reg no)
   IF_ID_C._subregOperand = _subregOperand;                 // Needed for lwl and lwr
   IF_ID_C._memControl = _memControl;                    // Memory instruction?
   IF_ID_C._writeREG = _writeREG; 
   IF_ID_C._writeFREG = _writeFREG;          // WB control
   IF_ID_C._branchOffset = _branchOffset;
   IF_ID_C._hiWPort = _hiWPort; 
   IF_ID_C._loWPort = _loWPort;             // WB control
   IF_ID_C._decodedShiftAmt = _decodedShiftAmt;               // Shift amount
   IF_ID_C._bd = _bd;                            // 1 if the next ins is delay slot
   IF_ID_C._btgt = _btgt;                          // branch target
   IF_ID_C._isSyscall = _isSyscall;                     // 1 if system call
   IF_ID_C._isIllegalOp = _isIllegalOp;                   // 1 if illegal opcode
   IF_ID_C._opControl = _opControl;
   IF_ID_C._memOp = _memOp;
}

/*
 *
 * Debugging: print registers
 *
 */
void 
Mipc::dumpregs (void)
{
   int i;

   printf ("\n--- PC = %08x ---\n", _pc);
   for (i=0; i < 32; i++) {
      if (i < 10)
	 printf (" r%d: %08x (%ld)\n", i, _gpr[i], _gpr[i]);
      else
	 printf ("r%d: %08x (%ld)\n", i, _gpr[i], _gpr[i]);
   }
   printf ("taken: %d, bd: %d\n", MEM_WB_C._btaken, MEM_WB_C._bd);
   printf ("target: %08x\n", MEM_WB_C._btgt);
}

void
Mipc::func_add_addu (Mipc *mc, unsigned ins)
{
   // printf("Encountered unimplemented instruction: add or addu.\n");
   // printf("You need to fill in func_add_addu in exec_helper.cc to proceed forward.\n");
   // exit(0);

   mc->ID_EX_C._opResultLo = (unsigned) (mc->ID_EX_C._decodedSRC1 + mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_and (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1 & mc->ID_EX_C._decodedSRC2;
}

void
Mipc::func_nor (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = ~(mc->ID_EX_C._decodedSRC1 | mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_or (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1 | mc->ID_EX_C._decodedSRC2;
}

void
Mipc::func_sll (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC2 << mc->ID_EX_C._decodedShiftAmt;
}

void
Mipc::func_sllv (Mipc *mc, unsigned ins)
{
   // printf("Encountered unimplemented instruction: sllv.\n");
   // printf("You need to fill in func_sllv in exec_helper.cc to proceed forward.\n");
   // exit(0);
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC2 << (mc->ID_EX_C._decodedSRC1 & 0x1f);
}

void
Mipc::func_slt (Mipc *mc, unsigned ins)
{
   if (mc->ID_EX_C._decodedSRC1 < mc->ID_EX_C._decodedSRC2) {
      mc->ID_EX_C._opResultLo = 1;
   }
   else {
      mc->ID_EX_C._opResultLo = 0;
   }
}

void
Mipc::func_sltu (Mipc *mc, unsigned ins)
{
   if ((unsigned)mc->ID_EX_C._decodedSRC1 < (unsigned)mc->ID_EX_C._decodedSRC2) {
      mc->ID_EX_C._opResultLo = 1;
   }
   else {
      mc->ID_EX_C._opResultLo = 0;
   }
}

void
Mipc::func_sra (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC2 >> mc->ID_EX_C._decodedShiftAmt;
}

void
Mipc::func_srav (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC2 >> (mc->ID_EX_C._decodedSRC1 & 0x1f);
}

void
Mipc::func_srl (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = (unsigned)mc->ID_EX_C._decodedSRC2 >> mc->ID_EX_C._decodedShiftAmt;
}

void
Mipc::func_srlv (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = (unsigned)mc->ID_EX_C._decodedSRC2 >> (mc->ID_EX_C._decodedSRC1 & 0x1f);
}

void
Mipc::func_sub_subu (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = (unsigned)mc->ID_EX_C._decodedSRC1 - (unsigned)mc->ID_EX_C._decodedSRC2;
}

void
Mipc::func_xor (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1 ^ mc->ID_EX_C._decodedSRC2;
}

void
Mipc::func_div (Mipc *mc, unsigned ins)
{
   if (mc->ID_EX_C._decodedSRC2 != 0) {
      mc->ID_EX_C._opResultHi = (unsigned)(mc->ID_EX_C._decodedSRC1 % mc->ID_EX_C._decodedSRC2);
      mc->ID_EX_C._opResultLo = (unsigned)(mc->ID_EX_C._decodedSRC1 / mc->ID_EX_C._decodedSRC2);
   }
   else {
      mc->ID_EX_C._opResultHi = 0x7fffffff;
      mc->ID_EX_C._opResultLo = 0x7fffffff;
   }
}

void
Mipc::func_divu (Mipc *mc, unsigned ins)
{
   if ((unsigned)mc->ID_EX_C._decodedSRC2 != 0) {
      mc->ID_EX_C._opResultHi = (unsigned)(mc->ID_EX_C._decodedSRC1) % (unsigned)(mc->ID_EX_C._decodedSRC2);
      mc->ID_EX_C._opResultLo = (unsigned)(mc->ID_EX_C._decodedSRC1) / (unsigned)(mc->ID_EX_C._decodedSRC2);
   }
   else {
      mc->ID_EX_C._opResultHi = 0x7fffffff;
      mc->ID_EX_C._opResultLo = 0x7fffffff;
   }
}

void
Mipc::func_mfhi (Mipc *mc, unsigned ins)
{  
#ifdef BYPASS_ENABLED
   if(mc->ID_EX_C._bypassSRC1 == BYPASS_EX_EX) {
      mc->ID_EX_C._opResultLo = mc->EX_MEM._opResultHi;
   }
#ifdef BYPASS_MEM_EX_ENABLED
   else if(mc->ID_EX_C._bypassSRC1 == BYPASS_MEM_EX) {
      mc->ID_EX_C._opResultLo = mc->MEM_WB._opResultHi;
   }
#endif
   else {
      mc->ID_EX_C._opResultLo = mc->_hi;
   }
#else
   mc->ID_EX_C._opResultLo = mc->_hi;
#endif
}

void
Mipc::func_mflo (Mipc *mc, unsigned ins)
{
#ifdef BYPASS_ENABLED
   if(mc->ID_EX_C._bypassSRC1 == BYPASS_EX_EX) {
      mc->ID_EX_C._opResultLo = mc->EX_MEM._opResultLo;
   }
#ifdef BYPASS_MEM_EX_ENABLED
   else if(mc->ID_EX_C._bypassSRC1 == BYPASS_MEM_EX) {
      mc->ID_EX_C._opResultLo = mc->MEM_WB._opResultLo;
   }
#endif
   else {
      mc->ID_EX_C._opResultLo = mc->_lo;
   }
#else
   mc->ID_EX_C._opResultLo = mc->_lo;
#endif
}

void
Mipc::func_mthi (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultHi = mc->ID_EX_C._decodedSRC1;
}

void
Mipc::func_mtlo (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1;
}

void
Mipc::func_mult (Mipc *mc, unsigned ins)
{
   unsigned int ar1, ar2, s1, s2, r1, r2, t1, t2;
                                                                                
   ar1 = mc->ID_EX_C._decodedSRC1;
   ar2 = mc->ID_EX_C._decodedSRC2;
   s1 = ar1 >> 31; if (s1) ar1 = 0x7fffffff & (~ar1 + 1);
   s2 = ar2 >> 31; if (s2) ar2 = 0x7fffffff & (~ar2 + 1);
                                                                                
   t1 = (ar1 & 0xffff) * (ar2 & 0xffff);
   r1 = t1 & 0xffff;              // bottom 16 bits
                                                                                
   // compute next set of 16 bits
   t1 = (ar1 & 0xffff) * (ar2 >> 16) + (t1 >> 16);
   t2 = (ar2 & 0xffff) * (ar1 >> 16);
                                                                                
   r1 = r1 | (((t1+t2) & 0xffff) << 16); // bottom 32 bits
   r2 = (ar1 >> 16) * (ar2 >> 16) + (t1 >> 16) + (t2 >> 16) +
            (((t1 & 0xffff) + (t2 & 0xffff)) >> 16);
                                                                                
   if (s1 ^ s2) {
      r1 = ~r1;
      r2 = ~r2;
      r1++;
      if (r1 == 0)
         r2++;
   }
   mc->ID_EX_C._opResultHi = r2;
   mc->ID_EX_C._opResultLo = r1;
}

void
Mipc::func_multu (Mipc *mc, unsigned ins)
{
   unsigned int ar1, ar2, s1, s2, r1, r2, t1, t2;
                                                                                
   ar1 = mc->ID_EX_C._decodedSRC1;
   ar2 = mc->ID_EX_C._decodedSRC2;
                                                                                
   t1 = (ar1 & 0xffff) * (ar2 & 0xffff);
   r1 = t1 & 0xffff;              // bottom 16 bits
                                                                                
   // compute next set of 16 bits
   t1 = (ar1 & 0xffff) * (ar2 >> 16) + (t1 >> 16);
   t2 = (ar2 & 0xffff) * (ar1 >> 16);
                                                                                
   r1 = r1 | (((t1+t2) & 0xffff) << 16); // bottom 32 bits
   r2 = (ar1 >> 16) * (ar2 >> 16) + (t1 >> 16) + (t2 >> 16) +
            (((t1 & 0xffff) + (t2 & 0xffff)) >> 16);
                            
   mc->ID_EX_C._opResultHi = r2;
   mc->ID_EX_C._opResultLo = r1;                                                    
}

void
Mipc::func_jalr (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._btaken = 1;
   mc->_num_jal++;
   mc->ID_EX_C._btgt = mc->ID_EX_C._decodedSRC1;
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._pc + 8;
}

void
Mipc::func_jr (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._btaken = 1;
   mc->_num_jr++;
   mc->ID_EX_C._btgt = mc->ID_EX_C._decodedSRC1;
}

void
Mipc::func_await_break (Mipc *mc, unsigned ins)
{
}

void
Mipc::func_syscall (Mipc *mc, unsigned ins)
{
   mc->fake_syscall (ins, mc->MEM_WB_C._pc);
}

void
Mipc::func_addi_addiu (Mipc *mc, unsigned ins)
{
   // printf("Encountered unimplemented instruction: addi or addiu.\n");
   // printf("You need to fill in func_addi_addiu in exec_helper.cc to proceed forward.\n");
   // exit(0);

   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._opResultLo = (unsigned) (mc->ID_EX_C._decodedSRC1 + mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_andi (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1 & mc->ID_EX_C._decodedSRC2;
}

void
Mipc::func_lui (Mipc *mc, unsigned ins)
{
   // printf("Encountered unimplemented instruction: lui.\n");
   // printf("You need to fill in func_lui in exec_helper.cc to proceed forward.\n");
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC2 << 16;
   // exit(0);
}

void
Mipc::func_ori (Mipc *mc, unsigned ins)
{
   // printf("Encountered unimplemented instruction: ori.\n");
   // printf("You need to fill in func_ori in exec_helper.cc to proceed forward.\n");
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1 | mc->ID_EX_C._decodedSRC2;
   // exit(0);
}

void
Mipc::func_slti (Mipc *mc, unsigned ins)
{
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   if (mc->ID_EX_C._decodedSRC1 < mc->ID_EX_C._decodedSRC2) {
      mc->ID_EX_C._opResultLo = 1;
   }
   else {
      mc->ID_EX_C._opResultLo = 0;
   }
}

void
Mipc::func_sltiu (Mipc *mc, unsigned ins)
{
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   if ((unsigned)mc->ID_EX_C._decodedSRC1 < (unsigned)mc->ID_EX_C._decodedSRC2) {
      mc->ID_EX_C._opResultLo = 1;
   }
   else {
      mc->ID_EX_C._opResultLo = 0;
   }
}

void
Mipc::func_xori (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1 ^ mc->ID_EX_C._decodedSRC2;
}

void
Mipc::func_beq (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   // printf("Encountered unimplemented instruction: beq.\n");
   // printf("You need to fill in func_beq in exec_helper.cc to proceed forward.\n");
   // exit(0);
   mc->ID_EX_C._btaken = (mc->ID_EX_C._decodedSRC1 == mc->ID_EX_C._decodedSRC2) ? 1: 0;
}

void
Mipc::func_bgez (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = !(mc->ID_EX_C._decodedSRC1 >> 31);
}

void
Mipc::func_bgezal (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = !(mc->ID_EX_C._decodedSRC1 >> 31);
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._pc + 8;
}

void
Mipc::func_bltzal (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = (mc->ID_EX_C._decodedSRC1 >> 31);
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._pc + 8;
}

void
Mipc::func_bltz (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = (mc->ID_EX_C._decodedSRC1 >> 31);
}

void
Mipc::func_bgtz (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = (mc->ID_EX_C._decodedSRC1 > 0);
}

void
Mipc::func_blez (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = (mc->ID_EX_C._decodedSRC1 <= 0);
}

void
Mipc::func_bne (Mipc *mc, unsigned ins)
{
   mc->_num_cond_br++;
   mc->ID_EX_C._btaken = (mc->ID_EX_C._decodedSRC1 != mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_j (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._btaken = 1;
}

void
Mipc::func_jal (Mipc *mc, unsigned ins)
{
   mc->_num_jal++;
   // printf("Encountered unimplemented instruction: jal.\n");
   // printf("You need to fill in func_jal in exec_helper.cc to proceed forward.\n");
   // exit(0);
   mc->ID_EX_C._btaken = 1;
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._pc + 8;
}

void
Mipc::func_lb (Mipc *mc, unsigned ins)
{
   signed int a1;

   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lbu (Mipc *mc, unsigned ins)
{
   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lh (Mipc *mc, unsigned ins)
{
   signed int a1;
                                                                                
   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lhu (Mipc *mc, unsigned ins)
{
   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lwl (Mipc *mc, unsigned ins)
{
   signed int a1;
   unsigned s1;
                                                                                
   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lw (Mipc *mc, unsigned ins)
{
   mc->_num_load++;
   // printf("Encountered unimplemented instruction: lw.\n");
   // printf("You need to fill in func_lw in exec_helper.cc to proceed forward.\n");
   // exit(0);

   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1 + mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lwr (Mipc *mc, unsigned ins)
{
   unsigned ar1, s1;
                                                                                
   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_lwc1 (Mipc *mc, unsigned ins)
{
   mc->_num_load++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_swc1 (Mipc *mc, unsigned ins)
{
   mc->_num_store++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_sb (Mipc *mc, unsigned ins)
{
   mc->_num_store++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_sh (Mipc *mc, unsigned ins)
{
   mc->_num_store++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_swl (Mipc *mc, unsigned ins)
{
   unsigned ar1, s1;
                                                                                
   mc->_num_store++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_sw (Mipc *mc, unsigned ins)
{
   mc->_num_store++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_swr (Mipc *mc, unsigned ins)
{
   unsigned ar1, s1;
                                                                                
   mc->_num_store++;
   SIGN_EXTEND_IMM(mc->ID_EX_C._decodedSRC2);
   mc->ID_EX_C._MAR = (unsigned)(mc->ID_EX_C._decodedSRC1+mc->ID_EX_C._decodedSRC2);
}

void
Mipc::func_mtc1 (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1;
}

void
Mipc::func_mfc1 (Mipc *mc, unsigned ins)
{
   mc->ID_EX_C._opResultLo = mc->ID_EX_C._decodedSRC1;
}



void
Mipc::mem_lb (Mipc *mc)
{
   signed int a1;

   a1 = mc->_mem->BEGetByte(mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
   SIGN_EXTEND_BYTE(a1);
   mc->EX_MEM_C._opResultLo = a1;
}

void
Mipc::mem_lbu (Mipc *mc)
{
   mc->EX_MEM_C._opResultLo = mc->_mem->BEGetByte(mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
}

void
Mipc::mem_lh (Mipc *mc)
{
   signed int a1;

   a1 = mc->_mem->BEGetHalfWord(mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
   SIGN_EXTEND_IMM(a1);
   mc->EX_MEM_C._opResultLo = a1;
}

void
Mipc::mem_lhu (Mipc *mc)
{
   mc->EX_MEM_C._opResultLo = mc->_mem->BEGetHalfWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
}

void
Mipc::mem_lwl (Mipc *mc)
{
   signed int a1;
   unsigned s1;

   mc->EX_MEM_C._subregOperand = mc->_gpr[mc->EX_MEM_C._regSRC2];
   a1 = mc->_mem->BEGetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
   s1 = (mc->EX_MEM_C._MAR & 3) << 3;
   mc->EX_MEM_C._opResultLo = (a1 << s1) | (mc->EX_MEM_C._subregOperand & ~(~0UL << s1));
}

void
Mipc::mem_lw (Mipc *mc)
{
   mc->EX_MEM_C._opResultLo = mc->_mem->BEGetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
}

void
Mipc::mem_lwr (Mipc *mc)
{
   unsigned ar1, s1;

   mc->EX_MEM_C._subregOperand = mc->_gpr[mc->EX_MEM_C._regSRC2];
   ar1 = mc->_mem->BEGetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
   s1 = (~mc->EX_MEM_C._MAR & 3) << 3;
   mc->EX_MEM_C._opResultLo = (ar1 >> s1) | (mc->EX_MEM_C._subregOperand & ~(~(unsigned)0 >> s1));
}

void
Mipc::mem_lwc1 (Mipc *mc)
{
   mc->EX_MEM_C._opResultLo = mc->_mem->BEGetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
}

void
Mipc::mem_swc1 (Mipc *mc)
{
   mc->_mem->Write(mc->EX_MEM_C._MAR & ~(LL)0x7, mc->_mem->BESetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7), mc->_fpr[mc->EX_MEM_C._decodedDST>>1].l[FP_TWIDDLE^(mc->EX_MEM_C._decodedDST&1)]));
}

void
Mipc::mem_sb (Mipc *mc)
{
   mc->_mem->Write(mc->EX_MEM_C._MAR & ~(LL)0x7, mc->_mem->BESetByte (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7), mc->_gpr[mc->EX_MEM_C._decodedDST] & 0xff));
}

void
Mipc::mem_sh (Mipc *mc)
{
   mc->_mem->Write(mc->EX_MEM_C._MAR & ~(LL)0x7, mc->_mem->BESetHalfWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7), mc->_gpr[mc->EX_MEM_C._decodedDST] & 0xffff));
}

void
Mipc::mem_swl (Mipc *mc)
{
   unsigned ar1, s1;

   ar1 = mc->_mem->BEGetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
   s1 = (mc->EX_MEM_C._MAR & 3) << 3;
   ar1 = (mc->_gpr[mc->EX_MEM_C._decodedDST] >> s1) | (ar1 & ~(~(unsigned)0 >> s1));
   mc->_mem->Write(mc->EX_MEM_C._MAR & ~(LL)0x7, mc->_mem->BESetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7), ar1));
}

void
Mipc::mem_sw (Mipc *mc)
{
   mc->_mem->Write(mc->EX_MEM_C._MAR & ~(LL)0x7, mc->_mem->BESetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7), mc->_gpr[mc->EX_MEM_C._decodedDST]));
}

void
Mipc::mem_swr (Mipc *mc)
{
   unsigned ar1, s1;

   ar1 = mc->_mem->BEGetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7));
   s1 = (~mc->EX_MEM_C._MAR & 3) << 3;
   ar1 = (mc->_gpr[mc->EX_MEM_C._decodedDST] << s1) | (ar1 & ~(~0UL << s1));
   mc->_mem->Write(mc->EX_MEM_C._MAR & ~(LL)0x7, mc->_mem->BESetWord (mc->EX_MEM_C._MAR, mc->_mem->Read(mc->EX_MEM_C._MAR & ~(LL)0x7), ar1));
}
