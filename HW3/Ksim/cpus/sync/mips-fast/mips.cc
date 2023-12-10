#include "mips.h"
#include <assert.h>
#include "mips-irix5.h"

Mipc::Mipc (Mem *m) : _l('M')
{
   _mem = m;
   _sys = new MipcSysCall (this);	// Allocate syscall layer

#ifdef MIPC_DEBUG
   _debugLog = fopen("mipc.debug", "w");
   assert(_debugLog != NULL);
#endif

   Reboot (ParamGetString ("Mipc.BootROM"));
}

Mipc::~Mipc (void)
{

}

void 
Mipc::MainLoop (void)
{
   LL addr;
   unsigned int ins;	// Local instruction register 

   Assert (_boot, "Mipc::MainLoop() called without boot?");

   _nfetched = 0;

   while (!_sim_exit) {
      AWAIT_P_PHI0;	// @posedge
      AWAIT_P_PHI1;	// @negedge

         if(_isSyscallInPipe) {
            // if there is syscall which is issued, then halt the fetcher
            IF_ID.reset_regs();
            continue;
         }
         else if(_toStall) {
            // dont do anything, fetch the same ins again
            // Dont reset IF_ID struct, let the decoder again decode the same instruction and calculate dependencies
            continue;
         }
#ifdef BRANCH_INTERLOCK
         else if(_branchInterlock) {
            IF_ID.reset_regs();
            continue;
         }
#endif
         else{
            addr = _pc;
            ins = _mem->BEGetWord (addr, _mem->Read(addr & ~(LL)0x7));
   #ifdef MIPC_DEBUG
            fprintf(_debugLog, "<%llu> Fetched ins %#x from PC %#x\n", SIM_TIME, ins, _pc);
   #endif   
            IF_ID._pc = _pc;
            IF_ID._ins = ins;
            _nfetched++;
            _pc += 4;
         }
   }

   MipcDumpstats();
   Log::CloseLog();
   
#ifdef MIPC_DEBUG
   assert(_debugLog != NULL);
   fclose(_debugLog);
#endif

   exit(0);
}

void
Mipc::MipcDumpstats()
{
   Log l('*');
  l.startLogging = 0;

  l.print ("");
  l.print ("************************************************************");
  l.print ("");
  _nfetched--; // one extra ins counted at last syscall
  l.print ("Number of instructions: %llu", _nfetched);
  l.print ("Number of simulated cycles: %llu", SIM_TIME);
  l.print ("CPI: %.2f", ((double)SIM_TIME)/_nfetched);
#ifdef BYPASS_ENABLED
  l.print("Load-interlock: %d", _load_interlock_cycles);
  l.print("Load-delay interlock stalls: %f", ((double) _load_interlock_cycles)/SIM_TIME);
#endif
  l.print ("Int Conditional Branches: %llu", _num_cond_br);
  l.print ("Jump and Link: %llu", _num_jal);
  l.print ("Jump Register: %llu", _num_jr);
  l.print ("Number of fp instructions: %llu", _fpinst);
  l.print ("Number of loads: %llu", _num_load);
  l.print ("Number of syscall emulated loads: %llu", _sys->_num_load);
  l.print ("Number of stores: %llu", _num_store);
  l.print ("Number of syscall emulated stores: %llu", _sys->_num_store);

  l.print("-----------------------------Stats---------------------\n");
  l.print("Total Ins    %llu", _nfetched);
  l.print("Percentage Loads    %lf", (_num_load + _sys->_num_load)/(float)(_nfetched));
  l.print("Percentage Stores    %lf", (_num_store+ _sys->_num_store)/(float)(_nfetched));
  l.print("Percentage CB    %lf", (_num_cond_br)/(float)(_nfetched));
  l.print ("");

}

void 
Mipc::fake_syscall (unsigned int ins, unsigned int pc)
{  
#ifdef MIPC_DEBUG
   fprintf(_debugLog,"---------------Pc  in Fake Syscall: %#x--------------\n", pc);
#endif
   _sys->pc = pc;
   _sys->quit = 0;
   _sys->EmulateSysCall ();
   if (_sys->quit)
      _sim_exit = 1;
}

/*------------------------------------------------------------------------
 *
 *  Mipc::Reboot --
 *
 *   Reset processor state
 *
 *------------------------------------------------------------------------
 */
void 
Mipc::Reboot (char *image)
{
   FILE *fp;
   Log l('*');

   _boot = 0;

   if (image) {
      _boot = 1;
      printf ("Executing %s\n", image);
      fp = fopen (image, "r");
      if (!fp) {
	 fatal_error ("Could not open `%s' for booting host!", image);
      }
      _mem->ReadImage(fp);
      fclose (fp);

      // Reset state
      _isSyscallInPipe = FALSE;
      _toStall = FALSE;
      _isSubRegOps = FALSE;
#ifdef BRANCH_INTERLOCK
      _branchInterlock = FALSE;
#endif

      _num_load = 0;
      _num_store = 0;
      _fpinst = 0;
      _num_cond_br = 0;
      _num_jal = 0;
      _num_jr = 0;
      _load_interlock_cycles = 0;

      IF_ID.reset_regs();
      ID_EX.reset_regs();
      EX_MEM.reset_regs();
      MEM_WB.reset_regs();

      IF_ID_C.reset_regs();
      ID_EX_C.reset_regs();
      EX_MEM_C.reset_regs();
      MEM_WB_C.reset_regs();

      _sim_exit = 0;
      _pc = ParamGetInt ("Mipc.BootPC");	// Boom! GO
   }
}

LL
MipcSysCall::GetDWord(LL addr)
{
   _num_load++;      
   return m->Read (addr);
}

void
MipcSysCall::SetDWord(LL addr, LL data)
{
  
   m->Write (addr, data);
   _num_store++;
}

Word 
MipcSysCall::GetWord (LL addr) 
{ 
  
   _num_load++;   
   return m->BEGetWord (addr, m->Read (addr & ~(LL)0x7)); 
}

void 
MipcSysCall::SetWord (LL addr, Word data) 
{ 
  
   m->Write (addr & ~(LL)0x7, m->BESetWord (addr, m->Read(addr & ~(LL)0x7), data)); 
   _num_store++;
}
  
void 
MipcSysCall::SetReg (int reg, LL val) 
{ 
   _ms->_gpr[reg] = val; 
}

LL 
MipcSysCall::GetReg (int reg) 
{
   return _ms->_gpr[reg]; 
}

LL
MipcSysCall::GetTime (void)
{
  return SIM_TIME;
}
