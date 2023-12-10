#include "executor.h"
#include "mips.h"

Exe::Exe (Mipc *mc)
{
   _mc = mc;
}

Exe::~Exe (void) {}

#ifdef BYPASS_ENABLED
void Exe::updateInputArgs() {
   if(_mc->ID_EX_C._bypassSRC1 == BYPASS_EX_EX) {
      _mc->ID_EX_C._decodedSRC1 = _mc->EX_MEM._opResultLo;
   }
#ifdef BYPASS_MEM_EX_ENABLED
   else if(_mc->ID_EX_C._bypassSRC1 == BYPASS_MEM_EX) {
      _mc->ID_EX_C._decodedSRC1 = _mc->MEM_WB._opResultLo;
   }
#endif

   if(_mc->ID_EX_C._bypassSRC2 == BYPASS_EX_EX) {
      _mc->ID_EX_C._decodedSRC2 = _mc->EX_MEM._opResultLo;
   }
#ifdef BYPASS_MEM_EX_ENABLED
   else if(_mc->ID_EX_C._bypassSRC2 == BYPASS_MEM_EX) {
      _mc->ID_EX_C._decodedSRC2 = _mc->MEM_WB._opResultLo;
   }
#endif
}
#endif

void
Exe::MainLoop (void)
{
   unsigned int ins;
   Bool isSyscall, isIllegalOp;

   while (1) {
      AWAIT_P_PHI0;	// @posedge

         _mc->ID_EX_C = _mc->ID_EX;
         ins = _mc->ID_EX_C._ins;
         isSyscall = _mc->ID_EX_C._isSyscall;
         isIllegalOp = _mc->ID_EX_C._isIllegalOp;
#ifdef BRANCH_INTERLOCK
         _mc->_branchInterlock = FALSE;
#endif

         if (!isSyscall && !isIllegalOp) {
            if(_mc->ID_EX_C._opControl != NULL) {
#ifdef BYPASS_ENABLED
               updateInputArgs();
#endif
               _mc->ID_EX_C._opControl(_mc,ins);
            }
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Executed ins %#x\n", SIM_TIME, ins);
#endif
         }
         else if (isSyscall) {
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Deferring execution of syscall ins %#x\n", SIM_TIME, ins);
#endif
         }
         else {
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Illegal ins %#x in execution stage at PC %#x\n", SIM_TIME, ins, _mc->ID_EX_C._pc);
#endif
         }

         if (!isIllegalOp && !isSyscall) {

            // check whether the current ins is branch. If yes, set the branch Interlock
#ifdef BRANCH_INTERLOCK
            _mc->_branchInterlock = _mc->ID_EX_C._bd;
#endif

            if (_mc->ID_EX_C._bd && _mc->ID_EX_C._btaken)
            {  
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> ins %#x: -------------Branch Taken----------: SRC1: %lld, SRC2: %lld \n", SIM_TIME, ins, _mc->ID_EX_C._decodedSRC1, _mc->ID_EX_C._decodedSRC2);
#endif              
               // This pc will not get changed in case of branch interlock,
               // as there cannot be branch ins in branch delay slot
               _mc->_pc = _mc->ID_EX_C._btgt;
            }
         }

      AWAIT_P_PHI1;	// @negedge

         _mc->EX_MEM = _mc->ID_EX_C;
         
   }
}
