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
#include <unordered_set>
#include <vector>
#include <chrono>
#include <string_view>

using namespace std;

#define ONE_BILLION 1e+9

std::chrono::high_resolution_clock::time_point t_start;
std::chrono::high_resolution_clock::time_point t_end;
/* ================================================================== */
// Structures
/* ================================================================== */
enum INS_CATEGORY {
    LOADS = 0,
    STORES,
    NOP,
    DIRECTCALL,
    INDIRECTCALL,
    RETURNS,
    UNCONDITIONALBRANCH,
    CONDITIONALBRANCH,
    LOGICALOPS,
    ROTATEANDSHIFT,
    FLAGOPS,
    VECTORINS,
    CONDITIONALMOVES,
    MMXSSEINS,
    SYSCALLS,
    FPINS,
    OTHERS,

    TOTAL_CATEGORIES,
};

inline constexpr std::string_view categories_to_string[] = {
    "LOADS",
    "STORES",
    "NOP",
    "DIRECT_CALL",
    "INDIRECT_CALL",
    "RETURNS",
    "UNCONDITIONAL_BRANCH",
    "CONDITIONAL_BRANCH",
    "LOGICAL_OPS",
    "ROTATE_AND_SHIFT",
    "FLAG_OPS",
    "VECTOR_INS",
    "CONDITIONAL_MOVES",
    "MMX_SSE_INS",
    "SYSCALLS",
    "FP_INS",
    "OTHERS"
};

typedef struct distributionStats16{
    UINT32 insLenCount;
} distributionStats16;

typedef struct distributionStats8 {
    UINT32 numOperandsCount;
    UINT32 regReadOperandsCount;
    UINT32 regWriteOperandsCount;
} distributionStats8;

typedef struct distributionStats4 {
    UINT32 memOpsCount;
    UINT32 readMemOpsCount;
    UINT32 writeMemOpsCount;
} distributionStats4;

/* ================================================================== */
// Global variables
/* ================================================================== */

vector<UINT32> insCategoryCount(INS_CATEGORY::TOTAL_CATEGORIES, 0);
UINT32 insCategoryTotalCount = 0;
UINT32 CPI;

UINT64 fastForwardCount;
UINT64 insCount    = 0; //number of dynamically executed instructions
UINT64 analysedInsCount = 0;

unordered_set<UINT32> unqiue_data;
unordered_set<UINT32> unique_ins_set;
UINT32 uniq_ins_count;

vector<distributionStats16> distributionVec16(16, {0});
vector<distributionStats8> distributionVec8(8, {0});
vector<distributionStats4> distributionVec4(4, {0});

UINT32 maxBytesAccessed = 0;
UINT64 avgBytesAccessed = 0;
UINT32 totalMemIns = 0; // instruction having atleast one memory operands

INT32 maxImmediate = -INT_MAX;
INT32 minImmediate = INT_MAX;

ADDRDELTA minDisp = INT_MAX;
ADDRDELTA maxDisp = -INT_MAX;

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
inline void printElement(T t)
{
    *out << left << setw(30) << t;
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

// Predicated
VOID IncCategoryCounter(UINT32 counterNo, UINT32 incCount){
    insCategoryCount[counterNo] += incCount;
}

// Predicated
// data footprint
VOID DataFootPrint(UINT32 rwSize, VOID* addr){
    //here JAYA
    UINT32 addri = (UINT32)(addr);
    
    UINT32 start = (addri>>5);
    UINT32 end = ((addri+rwSize) >> 5);

    for(UINT32 i = start; i<=end; i++) {
        unqiue_data.insert(i);
    }
}

// Predicated
VOID PredicatedMinMax(UINT32 bb_totalBytesAccessed,
                     UINT32 bb_maxBytesAccessed,
                     UINT32 bb_memInsCount,
                     ADDRDELTA bb_maxDisp,
                     ADDRDELTA bb_minDisp)
{
    avgBytesAccessed += bb_totalBytesAccessed;
    if(bb_maxBytesAccessed > maxBytesAccessed){
        maxBytesAccessed = bb_maxBytesAccessed;
    }

    totalMemIns += bb_memInsCount;  // instruction having atleast one memory operands

    // Displacement
    if(bb_maxDisp > maxDisp)
        maxDisp = bb_maxDisp;

    if(bb_minDisp < minDisp) 
        minDisp = bb_minDisp;
}

// Predicated
void D4Count(UINT32 len,
            UINT32 memOpsCount,
            UINT32 readMemOpsCount,
            UINT32 writeMemOpsCount)
{
    distributionVec4[len].memOpsCount += memOpsCount;
    distributionVec4[len].readMemOpsCount += readMemOpsCount;
    distributionVec4[len].writeMemOpsCount += writeMemOpsCount;
}

// Non Predicated
void InstructionFootprint(UINT32 beginBlockNum, UINT32 numUniqueBlocks, UINT32 bb_numIns) {
    // // instruction footprint
    // if(numUniqueBlocks > 2) {
    //     uniq_ins_count += (numUniqueBlocks - 2); // all middle blocks will be unique and not shared by anyone
    // }

    // unique_ins_set.insert(beginBlockNum); // first block
    // unique_ins_set.insert(beginBlockNum+numUniqueBlocks-1); // last block

    for(UINT64 i = 0; i<numUniqueBlocks; i++) {
        unique_ins_set.insert(beginBlockNum+i);
    }

    analysedInsCount += bb_numIns;
}

// Non Predicated
void D16Count (UINT32 insLen, UINT32 insCount) {
    distributionVec16[insLen].insLenCount += insCount;
}

// Non Predicated
void D8Count(UINT32 len,
            UINT32 numOperandsCount,
            UINT32 regReadOperandsCount,
            UINT32 regWriteOperandsCount) {
    
    distributionVec8[len].numOperandsCount += numOperandsCount;
    distributionVec8[len].regReadOperandsCount += regReadOperandsCount;
    distributionVec8[len].regWriteOperandsCount += regWriteOperandsCount;
}

// Non Predicated
VOID NonPredicatedMinMax(INT32 bb_maxImm, INT32 bb_minImm) {
    if(bb_maxImm > maxImmediate) {
        maxImmediate = bb_maxImm;
    }

    if(bb_minImm < minImmediate) {
        minImmediate = bb_minImm;
    }
}

// Non Predicated
VOID CountIns(UINT32 numInstInBbl)
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
    printElement("Total Ins Count: "); printElement(insCount); *out << "\n";
    printElement("Analysed Ins Count: "); printElement(analysedInsCount); *out<< "\n";
    *out << endl;
    *out << endl;

    // Calculate total ins executed
    for(int i=0; i<INS_CATEGORY::TOTAL_CATEGORIES; i++) {
        insCategoryTotalCount += insCategoryCount[i];
    }

    // Print category stats in a file
    printElement("Category"); printElement("InsCount"); printElement("Percentage"); *out << endl;
    for(int i=0; i<INS_CATEGORY::TOTAL_CATEGORIES; i++) {
        printElement(categories_to_string[i]); printElement(insCategoryCount[i]); printElement((insCategoryCount[i]/(float)insCategoryTotalCount) * 100.0); *out << endl;
    }

    *out << endl;
    printElement("Total Ins"); printElement(insCategoryTotalCount); *out << endl;
    
    *out << endl;
    CPI = ceil(((insCategoryCount[INS_CATEGORY::LOADS] + insCategoryCount[INS_CATEGORY::STORES]) * 69.0 + (insCategoryTotalCount))/ insCategoryTotalCount);
    printElement("CPI: "); printElement(CPI); *out << endl;

    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    printElement("Instruction Footprint: "); printElement(unique_ins_set.size() + uniq_ins_count); *out << endl;
    printElement("Data Footprint: "); printElement(unqiue_data.size()); *out << endl;

    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    printElement("Distribution of instruction length"); *out<<endl;
    printElement("|"); printElement("Instruction Length"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec16.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec16[i].insLenCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Operands"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec8.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec8[i].numOperandsCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of register read operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num REG Read Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec8.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec8[i].regReadOperandsCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of register write operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num REG Write Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec8.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec8[i].regWriteOperandsCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of memory operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Memory Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec4.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec4[i].memOpsCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    printElement("Distribution of the number of memory read operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Memory Read Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec4.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec4[i].readMemOpsCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Distribution of the number of memory write operands in an instruction"); *out<<endl;
    printElement("|"); printElement("Num Memory Write Ops"); printElement("|"); printElement("Num Ins"); printElement("|"); *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    for(unsigned int i=0; i<distributionVec4.size(); i++) {
        printElement("|"); printElement(i); printElement("|"); printElement(distributionVec4[i].writeMemOpsCount); printElement("|"); *out<<endl;
    }
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Memory Bytes Touched"); *out << endl;
    printElement("Maximum: "); printElement(maxBytesAccessed); *out << endl;
    printElement("Average: "); printElement(avgBytesAccessed/((float)totalMemIns)); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    printElement("Immediate Field value"); *out << endl;
    printElement("Maximum: "); printElement(maxImmediate); *out << endl;
    printElement("Minimum: "); printElement(minImmediate); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    printElement("Displacement Field Value"); *out << endl;
    printElement("Maximum: "); printElement(maxDisp); *out << endl;
    printElement("Minimum: "); printElement(minDisp); *out << endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    
    t_end = std::chrono::high_resolution_clock::now();

    auto elapsed_time_s = std::chrono::duration_cast<std::chrono::seconds>(t_end-t_start).count();
    *out << endl;
    *out << endl;
    printElement("Total Execution Time: "); printElement(elapsed_time_s); *out<< endl;
    
    exit(0);
}


/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

inline void CategoryCount(BBL bbl) {
   
    INS bb_firstIns = BBL_InsHead(bbl);
    vector<distributionStats16> bb_dVec16(16, {0});
    vector<distributionStats8> bb_dVec8(8, {0});
    vector<distributionStats4> bb_dVec4(8, {0});
    vector<UINT32> bb_insCategoryCount(INS_CATEGORY::TOTAL_CATEGORIES, 0);

    UINT32 bb_startInsBlock = (((UINT32)(INS_Address(bb_firstIns)))>>5);
    unordered_set<UINT32> bb_uniqInsBlock;

    INT32 bb_maxImm = -INT_MAX;
    INT32 bb_minImm = INT_MAX;

    ADDRDELTA bb_maxDisp = -INT_MAX;
    ADDRDELTA bb_minDisp = INT_MAX;

    UINT32 bb_totalBytesAccessed = 0;
    UINT32 bb_maxBytesAccessed = 0;
    UINT32 bb_memInsCount = 0;

    for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
    {   
        
        UINT32 insSize = INS_Size(ins);

        // Find distribution stats
        bb_dVec16[insSize].insLenCount += 1;
        bb_dVec8[INS_OperandCount(ins)].numOperandsCount += 1;
        bb_dVec8[INS_MaxNumRRegs(ins)].regReadOperandsCount += 1;
        bb_dVec8[INS_MaxNumWRegs(ins)].regWriteOperandsCount += 1;

        
        // Operand and register count can be done on a basic block level, just store the count in a vector and call those many calls at the BB level
        /*
            Ins Footprint can be also done at a basic block level
            * We know that the basic block instructions are continuous in memory with no jumps(so PC+=insSize)
            * So calculate number of unique i-blocks accessed in a BB with the starting i-block number.
            * Send (i-block-start, num-i-blocks) to analysis routine which will insert in a set.
        */

        // Find instruction footprint
        UINT32 iaddr = (UINT32)(INS_Address(ins));
        UINT32 start = (iaddr>>5);
        UINT32 end = ((iaddr+insSize) >> 5);

        for(UINT32 i = start; i<=end; i++) {
            bb_uniqInsBlock.insert(i);
        }
     
        // can  be done at BB level, find max and min in a BB
        for(UINT32 op =0; op < INS_OperandCount(ins); op++) {
            if(INS_OperandIsImmediate(ins, op)){
                INT32 val = INS_OperandImmediate(ins, op);
                if(val > bb_maxImm) {
                    bb_maxImm = val;
                }

                if(val < bb_minImm) {
                    bb_minImm = val;
                }
            }
        }
        
        // Count load and store instructions for type B
        UINT32 memOperands = INS_MemoryOperandCount(ins);
        UINT32 loadOperands = 0, storeOperands = 0;
        UINT32 memBytesAccessed = 0;

        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {   
            UINT32 rwSize = INS_MemoryOperandSize(ins, memOp);

            // min nd max disp
            ADDRDELTA displacement = INS_OperandMemoryDisplacement(ins, memOp);
            if(displacement > bb_maxDisp)
                bb_maxDisp = displacement;

            if(displacement < bb_minDisp) 
                bb_minDisp = displacement;
            
            // Data Footprint(Will  be same for both read annd write operand)
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins,
                IPOINT_BEFORE,
                (AFUNPTR)DataFootPrint,
                IARG_UINT32, rwSize,
                IARG_MEMORYOP_EA, memOp,
                IARG_END
            );

            if (INS_MemoryOperandIsRead(ins, memOp))
            {   
                loadOperands++;
                memBytesAccessed += rwSize;
                bb_insCategoryCount[INS_CATEGORY::LOADS] += ((rwSize+3) >> 2);
            }

            // Note that in some architectures a single memory operand can be 
            // both read and written (for instance incl (%eax) on IA-32)
            // In that case we instrument it once for read and once for write.
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {   
                storeOperands++;
                memBytesAccessed += rwSize;
                bb_insCategoryCount[INS_CATEGORY::STORES] += ((rwSize+3) >> 2);
            }
        }
        
        if(memBytesAccessed) {
            bb_totalBytesAccessed += memBytesAccessed;
            if(memBytesAccessed > bb_maxBytesAccessed) {
                bb_maxBytesAccessed = memBytesAccessed;
            }

            bb_memInsCount += 1;
        }

        bb_dVec4[loadOperands+storeOperands].memOpsCount += 1;
        bb_dVec4[loadOperands].readMemOpsCount += 1;
        bb_dVec4[storeOperands].writeMemOpsCount += 1;

        // Categorize all instructions for type A
        UINT32 category = INS_Category(ins);

        switch(category) {
            case XED_CATEGORY_NOP:
            {   
                bb_insCategoryCount[INS_CATEGORY::NOP] += 1;
                break;
            }
            case XED_CATEGORY_CALL: 
            {
                if(INS_IsDirectCall(ins)) {
                    // Increment direct call count by one   
                    bb_insCategoryCount[INS_CATEGORY::DIRECTCALL] += 1;                
                }
                else {  
                    
                    bb_insCategoryCount[INS_CATEGORY::INDIRECTCALL] += 1;               
                }

                break;
            }
            case XED_CATEGORY_RET: 
            {   
                bb_insCategoryCount[INS_CATEGORY::RETURNS] += 1;                    
                break;
            }
            case XED_CATEGORY_UNCOND_BR: 
            {   
                bb_insCategoryCount[INS_CATEGORY::UNCONDITIONALBRANCH] += 1;                    
                break;
            }
            case XED_CATEGORY_COND_BR: 
            {   
                bb_insCategoryCount[INS_CATEGORY::CONDITIONALBRANCH] += 1;                    
                break;
            }
            case XED_CATEGORY_LOGICAL: 
            {   
                bb_insCategoryCount[INS_CATEGORY::LOGICALOPS] += 1;                    
                break;
            }
            case XED_CATEGORY_ROTATE:
            case XED_CATEGORY_SHIFT:
            {   
                bb_insCategoryCount[INS_CATEGORY::ROTATEANDSHIFT] += 1;
                break;
            }
            case XED_CATEGORY_FLAGOP: 
            {   
                bb_insCategoryCount[INS_CATEGORY::FLAGOPS] += 1;                    
                break;
            }
            case XED_CATEGORY_AVX:
            case XED_CATEGORY_AVX2:
            case XED_CATEGORY_AVX2GATHER:
            case XED_CATEGORY_AVX512:
            {   
                bb_insCategoryCount[INS_CATEGORY::VECTORINS] += 1;
                break;
            }
            case XED_CATEGORY_CMOV: 
            {   
                bb_insCategoryCount[INS_CATEGORY::CONDITIONALMOVES] += 1;                    
                break;
            }
            case XED_CATEGORY_MMX:
            case XED_CATEGORY_SSE:
            {   
                bb_insCategoryCount[INS_CATEGORY::MMXSSEINS] += 1;
                break;
            }
            case XED_CATEGORY_SYSCALL: 
            {   
                bb_insCategoryCount[INS_CATEGORY::SYSCALLS] += 1;                    
                break;
            }
            case XED_CATEGORY_X87_ALU: 
            {   
                bb_insCategoryCount[INS_CATEGORY::FPINS] += 1;                    
                break;
            }
            default:
            {   
                bb_insCategoryCount[INS_CATEGORY::OTHERS] += 1;
                break;
            }
        }
    }

    // Insert calls for counting categories
    for(int i=0; i<INS_CATEGORY::TOTAL_CATEGORIES; i++) {
        if(bb_insCategoryCount[i]){
            INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                bb_firstIns,
                IPOINT_BEFORE,
                (AFUNPTR)IncCategoryCounter,
                IARG_UINT32, i,
                IARG_UINT32, bb_insCategoryCount[i],
                IARG_END
            );
        }
    }

    // Instruction Footprint
    INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall (bb_firstIns, 
                        IPOINT_BEFORE, 
                        (AFUNPTR)InstructionFootprint,
                        IARG_UINT32, bb_startInsBlock, 
                        IARG_UINT32, bb_uniqInsBlock.size(),
                        IARG_UINT32, BBL_NumIns(bbl),
                        IARG_END);

    // Distribution stats
    // For len 16 vectors
    for(size_t i=0; i<bb_dVec16.size(); i++) {
        if(bb_dVec16[i].insLenCount) {
            INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall( bb_firstIns,
                                IPOINT_BEFORE,
                                (AFUNPTR) D16Count,
                                IARG_UINT32, i,
                                IARG_UINT32, bb_dVec16[i].insLenCount,
                                IARG_END
            );
        }
    }

    // for len8 vectors
    for(size_t i=0; i<bb_dVec8.size(); i++) {
        INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall( bb_firstIns,
                            IPOINT_BEFORE,
                            (AFUNPTR) D8Count,
                            IARG_UINT32, i,
                            IARG_UINT32, bb_dVec8[i].numOperandsCount,
                            IARG_UINT32, bb_dVec8[i].regReadOperandsCount,
                            IARG_UINT32, bb_dVec8[i].regWriteOperandsCount,
                            IARG_END
        );
    }

    // for len4 vectors
    for(size_t i=0; i<bb_dVec4.size(); i++) {
        INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenPredicatedCall( bb_firstIns,
                            IPOINT_BEFORE,
                            (AFUNPTR) D4Count,
                            IARG_UINT32, i,
                            IARG_UINT32, bb_dVec4[i].memOpsCount,
                            IARG_UINT32, bb_dVec4[i].readMemOpsCount,
                            IARG_UINT32, bb_dVec4[i].writeMemOpsCount,
                            IARG_END
        );
    }

    // Max and min findings

    // Max and avg bytes accessed/ Max and min displacement
    INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall( bb_firstIns,
                        IPOINT_BEFORE,
                        (AFUNPTR) PredicatedMinMax,
                        IARG_UINT32, bb_totalBytesAccessed,
                        IARG_UINT32, bb_maxBytesAccessed,
                        IARG_UINT32, bb_memInsCount,
                        IARG_ADDRINT, bb_maxDisp,
                        IARG_ADDRINT, bb_minDisp,
                        IARG_END
    );

    INS_InsertIfCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenCall( bb_firstIns,
                        IPOINT_BEFORE,
                        (AFUNPTR) NonPredicatedMinMax,
                        IARG_ADDRINT, bb_maxImm,
                        IARG_ADDRINT, bb_minImm,
                        IARG_END
    );

    // insert on BB level before the first instruction in the BB.
    INS_InsertCall(bb_firstIns, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
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

        // Insert a call to CountBbl() before every basic block, passing the number of instructions
        // BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
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

    t_start = std::chrono::high_resolution_clock::now();
    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
