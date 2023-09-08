/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <math.h>

using namespace std;

#define ONE_BILLION 1e+9

/* ================================================================== */
// Structures
/* ================================================================== */

typedef struct InsCategoryCount {
    // Will be instrumented for only true predicates
    UINT64 loads;
    UINT64 stores;
    UINT64 nop;
    UINT64 directCall;
    UINT64 indirectCall;
    UINT64 returns;
    UINT64 unconditionalBranch;
    UINT64 conditionalBranch;
    UINT64 logicalOps;
    UINT64 rotateAndShift;
    UINT64 flagOps;
    UINT64 vectorIns;
    UINT64 conditionalMoves;
    UINT64 mmxSSEIns;
    UINT64 syscalls;
    UINT64 fpIns;
    UINT64 others;
} InsCategoryCount;

/* ================================================================== */
// Global variables
/* ================================================================== */

InsCategoryCount insCategoryCount = {0}; // dynamic count of different category ins executed
UINT64 insCategoryTotalCount = 0;

UINT64 fastForwardCount;
UINT64 insCount    = 0; //number of dynamically executed instructions

std::ostream* out = &cerr;
string outputFile;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB< INT64 > KnobFastForwardCount(KNOB_MODE_WRITEONCE, "pintool", "f", "", "specify fast forward amount in multiples of 1 billion");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the dynamic counts/percentages of " << endl
         << "instructions executed in different spec2006 benchmark programs." << endl
         << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

template<typename T> 
void printElement(T t)
{
    *out << left << setw(50) << t;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   numInstInBbl    number of instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID IncCount(UINT64* count, UINT64 storeCount, UINT64 loadCount){
    (*count) ++;
    insCategoryCount.loads += loadCount;
    insCategoryCount.stores += storeCount;
    insCategoryTotalCount += (1 + storeCount + loadCount);
}

VOID CountIns(UINT64 numInstInBbl)
{
    insCount += numInstInBbl;
}

// Terminate condition
ADDRINT Terminate(void)
{
    return (insCount >= fastForwardCount + ONE_BILLION);
}

// Analysis routine to check fast-forward condition
ADDRINT FastForward (void) {
	return ((insCount >= fastForwardCount) && insCount);
}

// Analysis routine to exit the application
void MyExitRoutine (void) {
	// Do an exit system call to exit the application.
	// As we are calling the exit system call PIN would not be able to instrument application end.
	// Because of this, even if you are instrumenting the application end, the Fini function would not
	// be called. Thus you should report the statistics here, before doing the exit system call.

    // Print the stats here and then exit
    
    // Print category stats in a file
    printElement("Category"); printElement("InsCount"); printElement("Percentage"); *out << endl;
    printElement("Loads"); printElement(insCategoryCount.loads); printElement((insCategoryCount.loads/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Stores"); printElement(insCategoryCount.stores); printElement((insCategoryCount.stores/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Nop"); printElement(insCategoryCount.nop); printElement((insCategoryCount.nop/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("DirectCall"); printElement(insCategoryCount.directCall); printElement((insCategoryCount.directCall/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("IndirectCall"); printElement(insCategoryCount.indirectCall); printElement((insCategoryCount.indirectCall/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Returns"); printElement(insCategoryCount.returns); printElement((insCategoryCount.returns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("UnconditionalBranch"); printElement(insCategoryCount.unconditionalBranch); printElement((insCategoryCount.unconditionalBranch/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("ConditionalBranch"); printElement(insCategoryCount.conditionalBranch); printElement((insCategoryCount.conditionalBranch/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("LogicalOps"); printElement(insCategoryCount.logicalOps); printElement((insCategoryCount.logicalOps/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("RotateAndShift"); printElement(insCategoryCount.rotateAndShift); printElement((insCategoryCount.rotateAndShift/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("FlagOps"); printElement(insCategoryCount.flagOps); printElement((insCategoryCount.flagOps/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("VectorIns"); printElement(insCategoryCount.vectorIns); printElement((insCategoryCount.vectorIns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("ConditionalMoves"); printElement(insCategoryCount.conditionalMoves); printElement((insCategoryCount.conditionalMoves/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("MmxSSEIns"); printElement(insCategoryCount.mmxSSEIns); printElement((insCategoryCount.mmxSSEIns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Syscalls"); printElement(insCategoryCount.syscalls); printElement((insCategoryCount.syscalls/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("FpIns"); printElement(insCategoryCount.fpIns); printElement((insCategoryCount.fpIns/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Others"); printElement(insCategoryCount.others); printElement((insCategoryCount.others/(float)insCategoryTotalCount) * 100.0); *out << endl;
    printElement("Total Ins"); printElement(insCategoryTotalCount); *out << endl;
    
    exit(0);
}


/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

inline  void CategoryCount(BBL bbl) {

    for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
    {   
        // FastForward() is called for every instruction executed
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);

        // Count load and store instructions for type B
        UINT64 memOperands = INS_MemoryOperandCount(ins);
        UINT64 loadCount = 0, storeCount = 0;
        for (UINT64 memOp = 0; memOp < memOperands; memOp++)
        {   
            UINT64  rwSize = INS_MemoryOperandSize(ins, memOp);
            if (INS_MemoryOperandIsRead(ins, memOp))
            {   
                loadCount = ceil(rwSize/4.0);
            }
            // Note that in some architectures a single memory operand can be 
            // both read and written (for instance incl (%eax) on IA-32)
            // In that case we instrument it once for read and once for write.
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {   
                storeCount = ceil(rwSize/4.0);
            }
        }

        // Categorize all instructions for type A
        if(INS_Category(ins) == XED_CATEGORY_NOP) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.nop),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_CALL) {
            if(INS_IsDirectCall(ins)) {
                // Increment direct call count by one
                INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.directCall),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
            }
            else {
                INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.indirectCall),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
            }
        }
        else if(INS_Category(ins) == XED_CATEGORY_RET) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.returns),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_UNCOND_BR) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.unconditionalBranch),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_COND_BR) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.conditionalBranch),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_LOGICAL) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.logicalOps),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if((INS_Category(ins) == XED_CATEGORY_ROTATE) || (INS_Category(ins) == XED_CATEGORY_SHIFT)) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.rotateAndShift),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_FLAGOP) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.flagOps),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if((INS_Category(ins) == XED_CATEGORY_AVX) || (INS_Category(ins) == XED_CATEGORY_AVX2) || (INS_Category(ins) == XED_CATEGORY_AVX2GATHER) || (INS_Category(ins) == XED_CATEGORY_AVX512)) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.vectorIns),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_CMOV) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.conditionalMoves),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if((INS_Category(ins) == XED_CATEGORY_MMX) || (INS_Category(ins) == XED_CATEGORY_SSE)) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.mmxSSEIns),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_SYSCALL) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.syscalls),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else if(INS_Category(ins) == XED_CATEGORY_X87_ALU) {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.fpIns),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
        else {
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)IncCount,
                IARG_PTR,
                &(insCategoryCount.others),
                IARG_UINT64,
                storeCount,
                IARG_UINT64,
                loadCount,
                IARG_END);
        }
    }
}

/*!
 * Insert call to the CountBbl() analysis routine before every basic block 
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID* v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {   
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

        // MyExitRoutine() is called only when the last call returns a non-zero value.
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine, IARG_END);

        // Insert all the calls here once you have fast forwarded the given amount of ins
        CategoryCount(bbl);

        // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_UINT64, BBL_NumIns(bbl), IARG_END);
    }
}


/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 * 
 * This function will never be called in our case. We will use exit condition to terminate the program.
 */
VOID Fini(INT32 code, VOID* v)
{
    MyExitRoutine();
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    outputFile = KnobOutputFile.Value();

    cout << "outFile: " << outputFile << endl;
    if (!outputFile.empty())
    {
        out = new std::ofstream(outputFile.c_str());
    }

    fastForwardCount = KnobFastForwardCount.Value();
    fastForwardCount *= ONE_BILLION;
    cout << "FF count: " << fastForwardCount << endl;

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
