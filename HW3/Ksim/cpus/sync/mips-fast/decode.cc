#include "decode.h"
#include "mips.h"

Decode::Decode (Mipc *mc)
{
   _mc = mc;
}

Decode::~Decode (void) {}

#ifdef BYPASS_ENABLED
Bool Decode::computeBypass() {
   Bool toStall = FALSE;

   if(_mc->IF_ID_C._requireFPArg) {
      // requires Floating Point Argument
      if(_mc->IF_ID_C._regSRC1 != DEFAULT_REG_VALUE) {

         // check whether the value required is being produced in EX stage as of now, if yes, then stall
         if((_mc->ID_EX._writeFREG &&  (_mc->IF_ID_C._regSRC1 == _mc->ID_EX._decodedDST)))
         {  
            if(_mc->ID_EX._memControl) {// if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_load_interlock_cycles ++;
            }
            else 
               _mc->IF_ID_C._bypassSRC1 = BYPASS_EX_EX;
         }
         // check whether the value required is being produced in MEM stage as of now, if yes, then stall
         else if((_mc->EX_MEM._writeFREG &&  (_mc->IF_ID_C._regSRC1 == _mc->EX_MEM._decodedDST))) {
#ifdef BYPASS_MEM_EX_ENABLED
            _mc->IF_ID_C._bypassSRC1 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }

      // Similarly check for SRC2
      if(_mc->IF_ID_C._regSRC2 != DEFAULT_REG_VALUE) {

         if((_mc->ID_EX._writeFREG &&  (_mc->IF_ID_C._regSRC2 == _mc->ID_EX._decodedDST))){
            if(_mc->ID_EX._memControl){ // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_load_interlock_cycles ++;
            }
            else 
               _mc->IF_ID_C._bypassSRC2 = BYPASS_EX_EX;
         }
         else if((_mc->EX_MEM._writeFREG &&  (_mc->IF_ID_C._regSRC2 == _mc->EX_MEM._decodedDST))) {
#ifdef BYPASS_MEM_EX_ENABLED
            _mc->IF_ID_C._bypassSRC2 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }
   }
   else {
      // Integer Ins
      if(_mc->IF_ID_C._regSRC1 != 0 && _mc->IF_ID_C._regSRC1 != DEFAULT_REG_VALUE) {
         if((_mc->ID_EX._writeREG &&  (_mc->IF_ID_C._regSRC1 == _mc->ID_EX._decodedDST)))
         {
            if(_mc->ID_EX._memControl){ // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_load_interlock_cycles ++;
            }
            else 
               _mc->IF_ID_C._bypassSRC1 = BYPASS_EX_EX;
         }
         else if((_mc->EX_MEM._writeREG &&  (_mc->IF_ID_C._regSRC1 == _mc->EX_MEM._decodedDST))) {
#ifdef BYPASS_MEM_EX_ENABLED
            _mc->IF_ID_C._bypassSRC1 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }

      // Integer Ins
      if(!_mc->_isSubRegOps && _mc->IF_ID_C._regSRC2 != 0 && _mc->IF_ID_C._regSRC2 != DEFAULT_REG_VALUE) {
         if((_mc->ID_EX._writeREG &&  (_mc->IF_ID_C._regSRC2 == _mc->ID_EX._decodedDST)))
         {
            if(_mc->ID_EX._memControl){ // if it is a load instructions, then do load interlock
               toStall = TRUE;
               _mc->_load_interlock_cycles ++;
            }
            else 
               _mc->IF_ID_C._bypassSRC2 = BYPASS_EX_EX;
         }
         else if((_mc->EX_MEM._writeREG &&  (_mc->IF_ID_C._regSRC2 == _mc->EX_MEM._decodedDST))) {
#ifdef BYPASS_MEM_EX_ENABLED
            _mc->IF_ID_C._bypassSRC2 = BYPASS_MEM_EX;
#else
            toStall = TRUE;
#endif
         }
      }
   }

   // Check for hi/lo registers
   if(_mc->IF_ID_C._requireLo) {
      if(_mc->ID_EX._loWPort) {
         _mc->IF_ID_C._bypassSRC1 = BYPASS_EX_EX;
      }
      else if(_mc->EX_MEM._loWPort){
#ifdef BYPASS_MEM_EX_ENABLED
         _mc->IF_ID_C._bypassSRC1 = BYPASS_MEM_EX;
#else
         toStall = TRUE;
#endif
      }
   }

   if(_mc->IF_ID_C._requireHi){
      if(_mc->ID_EX._hiWPort){
         _mc->IF_ID_C._bypassSRC1 = BYPASS_EX_EX;
      }
      else if(_mc->EX_MEM._hiWPort) {
#ifdef BYPASS_MEM_EX_ENABLED
         _mc->IF_ID_C._bypassSRC1 = BYPASS_MEM_EX;
#else
         toStall = TRUE;
#endif
      }
   }
      
   return toStall;
}

#else 
Bool Decode::detectStalls() {

   Bool toStall = FALSE;

   if(_mc->IF_ID_C._requireFPArg) {
      // requires Floating Point Argument
      if(_mc->IF_ID_C._regSRC1 != DEFAULT_REG_VALUE) {

         // check whether the value required is being produced in EX stage as of now, if yes, then stall
         if((_mc->ID_EX._writeFREG &&  (_mc->IF_ID_C._regSRC1 == _mc->ID_EX._decodedDST)))
         {
            toStall = TRUE;
         }
         
         // check whether the value required is being produced in MEM stage as of now, if yes, then stall
         if((_mc->EX_MEM._writeFREG &&  (_mc->IF_ID_C._regSRC1 == _mc->EX_MEM._decodedDST))) {
            toStall = TRUE;
         }
      }

      // Similarly check for SRC2
      if(_mc->IF_ID_C._regSRC2 != DEFAULT_REG_VALUE) {
         if((_mc->ID_EX._writeFREG &&  (_mc->IF_ID_C._regSRC2 == _mc->ID_EX._decodedDST)))
         {
            toStall = TRUE;
         }
          
         if((_mc->EX_MEM._writeFREG &&  (_mc->IF_ID_C._regSRC2 == _mc->EX_MEM._decodedDST))) {
            toStall = TRUE;
         }
      }
   }
   else {
      // Integer Ins
      if(_mc->IF_ID_C._regSRC1 != 0 && _mc->IF_ID_C._regSRC1 != DEFAULT_REG_VALUE) {
         if((_mc->ID_EX._writeREG &&  (_mc->IF_ID_C._regSRC1 == _mc->ID_EX._decodedDST)))
         {
            toStall = TRUE;
         }
          
         if((_mc->EX_MEM._writeREG &&  (_mc->IF_ID_C._regSRC1 == _mc->EX_MEM._decodedDST))) {
            toStall = TRUE;
         }
      }

      // Integer Ins
      if(_mc->IF_ID_C._regSRC2 != 0 && _mc->IF_ID_C._regSRC2 != DEFAULT_REG_VALUE) {
         if((_mc->ID_EX._writeREG &&  (_mc->IF_ID_C._regSRC2 == _mc->ID_EX._decodedDST)))
         {
            toStall = TRUE;
         }
          
         if((_mc->EX_MEM._writeREG &&  (_mc->IF_ID_C._regSRC2 == _mc->EX_MEM._decodedDST))) {
            toStall = TRUE;
         }
      }
   }

   // Check for hi/lo registers
   if(_mc->IF_ID_C._requireLo && (_mc->ID_EX._loWPort || _mc->EX_MEM._loWPort))
      toStall = TRUE;
   if(_mc->IF_ID_C._requireHi && (_mc->ID_EX._hiWPort || _mc->EX_MEM._hiWPort))
      toStall = TRUE;
      
   return toStall;
}
#endif

void
Decode::MainLoop (void)
{
   unsigned int ins;

   while (1) {
      AWAIT_P_PHI0;	// @posedge

      _mc->IF_ID_C = _mc->IF_ID; // copy the input

#ifdef BYPASS_ENABLED
      _mc->IF_ID_C._bypassSRC1 = BYPASS_NONE;
      _mc->IF_ID_C._bypassSRC2 = BYPASS_NONE;
#endif

      ins = _mc->IF_ID_C._ins;

      // check for data hazards
      _mc->Dec(ins, FALSE);

      if(!_mc->IF_ID_C._isIllegalOp) {
#ifdef BYPASS_ENABLED
         _mc->_toStall = computeBypass();
#else
         _mc->_toStall = detectStalls(); 
#endif
      }

      if(!_mc->_toStall && _mc->IF_ID_C._isSyscall){
         _mc->_isSyscallInPipe = TRUE;
      }
   
      AWAIT_P_PHI1;	// @negedge
         if(_mc->_toStall) {  
            _mc->ID_EX.reset_regs();
         }
         else {
            _mc->Dec(ins, TRUE); // just to make register file read at negedge
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, ins);
#endif
            _mc->ID_EX = _mc->IF_ID_C;
         }
   }
}
