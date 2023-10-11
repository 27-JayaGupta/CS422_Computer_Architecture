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
#include <list>
#include <chrono>

using namespace std;

#define ONE_BILLION 1e+9

std::chrono::high_resolution_clock::time_point t_start;
std::chrono::high_resolution_clock::time_point t_end;

/* ================================================================== */
// Global variables
/* ================================================================== */
vector<vector<int>> truth_table_2_bit(4, vector<int>(2, 0));
vector<vector<int>> truth_table_3_bit(8, vector<int>(2, 0));

UINT64 fastForwardCount;
UINT64 insCount    = 0; //number of dynamically executed instructions
// UINT64 prevInsCount = 0; 
UINT64 condBranchCount = 0;
UINT64 forwardCondBranchCount = 0;
UINT64 backwardCondBranchCount = 0;

std::ostream* out = &cerr;
string outputFile;

/* ================================================================== */
// Structures
/* ================================================================== */

class FNBT {
public:
    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;

    FNBT() {}

    inline void predictAndUpdate(bool actualPrediction, bool isForwardBranch) {
        // predict
        bool result = (actualPrediction != (!isForwardBranch));

        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;
    }
};

class BIM {

private:
    vector<int> pht;    // 2 bit counter

public:
    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;

    BIM() : pht(512, 0) {}

    // update is called only for conditional branches
    // so lastIdxChosen and lastPrediction will have appropriate values
    inline void predictAndUpdate(UINT32 pc, bool actualPrediction, bool isForwardBranch) {
        
        // predict
        int lastIdxChosen = pc & 0x1FF; // last 9 bits of pc to index into PHT
        bool lastPrediction = (pht[lastIdxChosen] >= 2);

        // update
        if(actualPrediction) {
            pht[lastIdxChosen] = min(pht[lastIdxChosen]+1, 3);
        }
        else{
            pht[lastIdxChosen] = max(pht[lastIdxChosen]-1, 0);
        }

        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;
    }

};

class SAg {

private:
    vector<int> bht;
    vector<int> pht; // 2 bit counter

public:
    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;

    SAg() : bht(1024, 0), pht(512, 0) {}

    // update is called only for conditional branches
    // so lastIdxChosen and lastPrediction will have appropriate values
    inline bool predictAndUpdate(UINT32 pc, bool actualPrediction, bool isForwardBranch) {
        
        // predict
        int lastBHTIdxChosen = pc & 0x3FF; // last 10 bits of pc to index into PHT
        int lastPHTIdxChosen = bht[lastBHTIdxChosen];
        bool lastPrediction = (pht[lastPHTIdxChosen] >= 2);

        // update PHT
        if(actualPrediction) {
            pht[lastPHTIdxChosen] = min(pht[lastPHTIdxChosen]+1, 3);
        }
        else{
            pht[lastPHTIdxChosen] = max(pht[lastPHTIdxChosen]-1, 0);
        }

        // update history in BHT
        bht[lastBHTIdxChosen] = (bht[lastBHTIdxChosen] << 1) | actualPrediction;
        bht[lastBHTIdxChosen] &= 0x1FF; // 9 bit history 
        
        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;

        return lastPrediction;
    }

};

class GlobalPred {

public:
    vector<int> pht; // 3 bit counter

    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;
    int ghr = 0; 

    GlobalPred() : pht(512, 0) {}
};

class GAg: public GlobalPred {

public:

    GAg() : GlobalPred() {}

    inline bool predictAndUpdate(bool actualPrediction, bool isForwardBranch) {

        // predict
        int lastPHTIdxChosen = ghr;
        bool lastPrediction = (pht[lastPHTIdxChosen] >= 4);

        // update PHT
        if(actualPrediction) {
            pht[lastPHTIdxChosen] = min(pht[lastPHTIdxChosen]+1, 7);
        }
        else{
            pht[lastPHTIdxChosen] = max(pht[lastPHTIdxChosen]-1, 0);
        }

        // update history in GHR
        ghr = (ghr << 1) | actualPrediction;
        ghr &= 0x1FF; // 9 bit history 
        
        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;

        return lastPrediction;
    }
};

class gshare: public GlobalPred {

public:

    gshare() : GlobalPred() {}

    inline bool predictAndUpdate(UINT32 pc, bool actualPrediction, bool isForwardBranch) {
        // predict
        int lastPHTIdxChosen = (ghr ^ pc) & 0x1FF;
        bool lastPrediction = (pht[lastPHTIdxChosen] >= 4);

        // update PHT
        if(actualPrediction) {
            pht[lastPHTIdxChosen] = min(pht[lastPHTIdxChosen]+1, 7);
        }
        else{
            pht[lastPHTIdxChosen] = max(pht[lastPHTIdxChosen]-1, 0);
        }

        // update history in GHR
        ghr = (ghr << 1) | actualPrediction;
        ghr &= 0x1FF; // 9 bit history 
        
        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;

        return lastPrediction;
    }
};

class SAg_GAg_Hybrid {

private:
    // SAg is 0, GAg is 1
    vector<int> metaPredictor; // 2 bit counter, indexed by GHR

public:
    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;

    SAg_GAg_Hybrid() : metaPredictor(512, 0) {}

    inline void predictAndUpdate(UINT32 ghr, bool SAgPrediction, bool GAgPrediction, bool actualPrediction, bool isForwardBranch) {

        // predict
        int lastIdxChosen = ghr;
        bool lastPrediction = (metaPredictor[lastIdxChosen] >= 2) ? GAgPrediction : SAgPrediction;

        // update only if output differs
        if(GAgPrediction != SAgPrediction) {
            bool favor = (GAgPrediction == actualPrediction);
            // metaPredictor[lastIdxChosen] = truth_table_2_bit[metaPredictor[lastIdxChosen]][favor];

            if(favor) {
                metaPredictor[lastIdxChosen] = min(metaPredictor[lastIdxChosen]+1, 3);
            }
            else{
                metaPredictor[lastIdxChosen] = max(metaPredictor[lastIdxChosen]-1, 0);
            }
        }

        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;
    }
};

class MAJ {

public:
    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;

    MAJ() {}

    inline void predictAndUpdate(bool first, bool second, bool third, bool actualPrediction, bool isForwardBranch) {
        // predict
        bool lastPrediction = (first && second) || (first && third) || (second && third);
        
        // updation
        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result;
    }
};

class TournamentMP {
private:
    vector<int> SAg_GAg; // 2 bit counter
    vector<int> SAg_gshare; // 2 bit counter
    vector<int> GAg_gshare; // 2 bit counter

public:
    int mispredictionCount = 0;
    int mispredictionCount_Frwd = 0;
    int mispredictionCount_Bcwd = 0;

    TournamentMP() : SAg_GAg(512, 0), SAg_gshare(512, 0), GAg_gshare(512, 0) {}

    inline void predictAndUpdate(UINT32 ghr, bool SAgPred, bool GAgPred, bool gsharePred, bool actualPrediction, bool isForwardBranch) {
        
        // predict
        int lastIdxChosen = ghr;
        bool isGAg = (SAg_GAg[lastIdxChosen] >= 2);
        bool isgshare = isGAg ? (GAg_gshare[lastIdxChosen] >= 2) : (SAg_gshare[lastIdxChosen] >= 2);
        bool lastPrediction = isgshare ? gsharePred : (isGAg ? GAgPred : SAgPred);

        // update counter tables
        if(GAgPred != SAgPred) {
            bool favor = (GAgPred == actualPrediction);
            if(favor) {
                SAg_GAg[lastIdxChosen] = min(SAg_GAg[lastIdxChosen]+1, 3);
            }
            else{
                SAg_GAg[lastIdxChosen] = max(SAg_GAg[lastIdxChosen]-1, 0);
            }
        }

        if(GAgPred != gsharePred) {
            bool favor = (gsharePred == actualPrediction);
            if(favor) {
                GAg_gshare[lastIdxChosen] = min(GAg_gshare[lastIdxChosen]+1, 3);
            }
            else{
                GAg_gshare[lastIdxChosen] = max(GAg_gshare[lastIdxChosen]-1, 0);
            }
        }

        if(SAgPred != gsharePred) {
            bool favor = (gsharePred == actualPrediction);
            if(favor) {
                SAg_gshare[lastIdxChosen] = min(SAg_gshare[lastIdxChosen]+1, 3);
            }
            else{
                SAg_gshare[lastIdxChosen] = max(SAg_gshare[lastIdxChosen]-1, 0);
            }
        }

        bool result = (actualPrediction != lastPrediction);
        mispredictionCount += result;
        if(isForwardBranch) mispredictionCount_Frwd += result;
        else mispredictionCount_Bcwd += result; 
    }
};

class BTBBlock {
public:
    int tag;
    bool valid = false;
    UINT32 target;
};

class BTB1 {
private:
    int set;
    int ways;
    vector<list<int>> lru;

    inline void updateLRU(int setNo, int wayNo) {
        list<int> &l = lru[setNo];

        for(auto itr = l.begin(); itr != l.end(); itr++) {
            if(*itr == wayNo) {
                l.erase(itr);
                l.push_front(wayNo);
                break;
            }
        }
    }

    inline void newEntry(int setNo, int wayNo, int tag, int target) {
        btb[setNo][wayNo].valid = true;
        btb[setNo][wayNo].target = target;
        btb[setNo][wayNo].tag = tag;

        updateLRU(setNo, wayNo);
    }

    inline int evict(int setNo) {
        return lru[setNo].back();
    }

public:
    vector<vector<BTBBlock>> btb;
    int btbAccess = 0;
    int btbMiss = 0;
    int mispredictionCount = 0;

    BTB1(int s, int w) : set(s), ways(w), btb(s, vector<BTBBlock>(w, {0})) {     
        for(int i=0; i<set; i++) {
            list<int> l;
            for(int j=0; j<ways; j++) l.push_back(j);
            lru.push_back(l);
        }
    }

    inline void lookUpAndUpdate(UINT32 pc, UINT32 insSize, UINT32 actualTarget) {
        btbAccess ++;
        int setNo = pc & (0x7F);
        int tag = (pc >> 7);
        bool btbHit = false;
        bool isEmptyBlock = false;
        int emptyWay;

        for(int w=0; w<ways; w++) {
            BTBBlock &block = btb[setNo][w];
            if(block.valid) {
                if(block.tag == tag) {
                    btbHit = true;
                    if(block.target != actualTarget) mispredictionCount++;
                    block.target = actualTarget;
                    updateLRU(setNo, w);
                }
            }
            else {
                isEmptyBlock = true;
                emptyWay = w;
            }
        }

        if(!btbHit){
            // next instruction
            btbMiss ++;
            UINT32 predTarget = pc + insSize;
            if(predTarget != actualTarget) mispredictionCount++;

            if(!isEmptyBlock) {
                emptyWay = evict(setNo);                
            }

            // make a new entry
            newEntry(setNo, emptyWay, tag, actualTarget);
        }
    }
};

class BTB2 {
private:
    int set;
    int ways;
    vector<list<int>> lru;

    inline void updateLRU(int setNo, int wayNo) {
        list<int> &l = lru[setNo];

        for(auto itr = l.begin(); itr != l.end(); itr++) {
            if(*itr == wayNo) {
                l.erase(itr);
                l.push_front(wayNo);
                break;
            }
        }
    }

    inline void newEntry(int setNo, int wayNo, int tag, int target) {
        btb[setNo][wayNo].valid = true;
        btb[setNo][wayNo].target = target;
        btb[setNo][wayNo].tag = tag;

        updateLRU(setNo, wayNo);
    }

    inline int evict(int setNo) {
        return lru[setNo].back();
    }

public:
    vector<vector<BTBBlock>> btb;
    int btbAccess = 0;
    int btbMiss = 0;
    int mispredictionCount = 0;

    BTB2(int s, int w) : set(s), ways(w), btb(s, vector<BTBBlock>(w, {0})) {     
        for(int i=0; i<set; i++) {
            list<int> l;
            for(int j=0; j<ways; j++) l.push_back(j);
            lru.push_back(l);
        }
    }

    inline void lookUpAndUpdate(UINT32 ghr, UINT32 pc, UINT32 insSize, UINT32 actualTarget) {
        btbAccess ++;
        int setNo = (pc ^ ghr) & 0x7F;
        int tag = pc;
        bool btbHit = false;
        bool isEmptyBlock = false;
        int emptyWay;

        for(int w=0; w<ways; w++) {
            BTBBlock &block = btb[setNo][w];
            if(block.valid) {
                if(block.tag == tag) {
                    btbHit = true;
                    if(block.target != actualTarget) mispredictionCount++;
                    block.target = actualTarget;
                    updateLRU(setNo, w);
                }
            }
            else {
                isEmptyBlock = true;
                emptyWay = w;
            }
        }

        if(!btbHit){
            // next instruction
            btbMiss ++;
            UINT32 predTarget = pc + insSize;
            if(predTarget != actualTarget) mispredictionCount++;

            if(!isEmptyBlock) {
                emptyWay = evict(setNo);                
            }

            // make a new entry
            newEntry(setNo, emptyWay, tag, actualTarget);
        }
    }
};

/* ================================================================== */
// Global variables
/* ================================================================== */

// Predictors
FNBT __fnbt;
BIM __bim;
SAg __sag;
GAg __gag;
gshare __gshare;
SAg_GAg_Hybrid __sag_gag_hybrid;
MAJ __maj;
TournamentMP __tournamentMP;

BTB1 __btb1(128, 4);
BTB2 __btb2(128, 4);

bool isLastInsCond = false;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB< INT64 > KnobFastForwardCount(KNOB_MODE_WRITEONCE, "pintool", "f", "", "specify fast forward amount in multiples of 1 billion");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

inline void initTT(void) {
    truth_table_2_bit[0][0] = 0; 
    truth_table_2_bit[0][1] = 1;
    truth_table_2_bit[1][0] = 0;
    truth_table_2_bit[1][1] = 2;
    truth_table_2_bit[2][0] = 1;
    truth_table_2_bit[2][1] = 3;
    truth_table_2_bit[3][0] = 2;
    truth_table_2_bit[3][1] = 3;

    truth_table_3_bit[0][0] = 0;
    truth_table_3_bit[0][1] = 1;
    truth_table_3_bit[1][0] = 0;
    truth_table_3_bit[1][1] = 2;
    truth_table_3_bit[2][0] = 1;
    truth_table_3_bit[2][1] = 3;
    truth_table_3_bit[3][0] = 2;
    truth_table_3_bit[3][1] = 4;
    truth_table_3_bit[4][0] = 3;
    truth_table_3_bit[4][1] = 5;
    truth_table_3_bit[5][0] = 4;
    truth_table_3_bit[5][1] = 6;
    truth_table_3_bit[6][0] = 5;
    truth_table_3_bit[6][1] = 7;
    truth_table_3_bit[7][0] = 6;
    truth_table_3_bit[7][1] = 7;
}

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


// VOID CountIns(UINT32 numInstInBbl)
// {   
//     prevInsCount = insCount;
//     insCount += numInstInBbl;
// }

// Non Predicated
VOID CountIns()
{
    insCount ++;
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
    printElement("Total Ins Count: "); printElement(insCount); *out<<endl;
    *out << endl;

    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    *out << "Total conditional Branch Ins: " << condBranchCount << endl;
    *out << "Total Forward Conditional Branches: " << forwardCondBranchCount << endl;
    *out << "Total Backward Conditional Branches: " << backwardCondBranchCount << endl;

   printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    printElement("Direction Predictor"); printElement("Misprediction Frac(Forward)"); printElement("Misprediction Frac(Backward)"); printElement("Misprediction Frac(Total)"); *out<<endl;
    
    printElement("FNBT");
    printElement((__fnbt.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__fnbt.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__fnbt.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("BIM");
    printElement((__bim.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__bim.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__bim.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("SAG");
    printElement((__sag.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__sag.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__sag.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("GAG");
    printElement((__gag.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__gag.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__gag.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("GSHARE");
    printElement((__gshare.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__gshare.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__gshare.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("SAG_GAG_HYBRID");
    printElement((__sag_gag_hybrid.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__sag_gag_hybrid.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__sag_gag_hybrid.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("MAJ");
    printElement((__maj.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__maj.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__maj.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    printElement("TOURNAMENTMP");
    printElement((__tournamentMP.mispredictionCount_Frwd/(float) forwardCondBranchCount) * 100.0); 
    printElement((__tournamentMP.mispredictionCount_Bcwd/(float) backwardCondBranchCount) * 100.0);
    printElement((__tournamentMP.mispredictionCount/(float)condBranchCount) * 100.0); *out<<endl;

    
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    printElement("Total Indirect Calls"); printElement(__btb1.btbAccess); printElement(__btb2.btbAccess); *out << endl;
    *out << endl;

    printElement("BTB Type"); printElement("Misprediction Fraction"); printElement("BTB Miss Rate"); *out<<endl;

    printElement("BTB(PC)"); 
    printElement((__btb1.mispredictionCount/(float)__btb1.btbAccess) * 100.0); 
    printElement((__btb1.btbMiss/(float)__btb1.btbAccess) * 100.0); * out<<endl;

    printElement("BTB(PC XOR GHR)"); 
    printElement((__btb2.mispredictionCount/(float)__btb2.btbAccess) * 100.0); 
    printElement((__btb2.btbMiss/(float)__btb2.btbAccess) * 100.0); * out<<endl;

    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;
    t_end = std::chrono::high_resolution_clock::now();

    auto elapsed_time_s = std::chrono::duration_cast<std::chrono::seconds>(t_end-t_start).count();
    *out << endl;
    *out << endl;
    printElement("Total Execution Time: "); printElement(elapsed_time_s); *out<< endl;
    exit(0);
}

VOID ConditionalBranchAnalysis(UINT32 pc, bool branch_taken, UINT32 branchTarget) {

    bool isForwardBranch = false;
    condBranchCount ++;

    if(branchTarget < pc) backwardCondBranchCount++;
    else  {
        forwardCondBranchCount++;
        isForwardBranch = true;
    }

    int ghr = __gag.ghr;

    // prediction and updation
    __fnbt.predictAndUpdate(branch_taken, isForwardBranch);
    __bim.predictAndUpdate(pc, branch_taken, isForwardBranch);
    bool sag_pred = __sag.predictAndUpdate(pc, branch_taken, isForwardBranch);
    bool gag_pred = __gag.predictAndUpdate(branch_taken, isForwardBranch);
    bool gshare_pred = __gshare.predictAndUpdate(pc, branch_taken, isForwardBranch);
    __sag_gag_hybrid.predictAndUpdate(ghr, sag_pred, gag_pred, branch_taken, isForwardBranch);
    __maj.predictAndUpdate(sag_pred, gag_pred, gshare_pred, branch_taken, isForwardBranch);
    __tournamentMP.predictAndUpdate(ghr, sag_pred, gag_pred, gshare_pred, branch_taken, isForwardBranch);
}

VOID IndirectBranchPred(UINT32 pc, UINT32 insSize, UINT32 branchTarget) {
    __btb1.lookUpAndUpdate(pc, insSize, branchTarget);
    __btb2.lookUpAndUpdate(__gag.ghr, pc, insSize, branchTarget);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

inline void AnalyzeBB(BBL bbl) {
   
    for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
    {
        // for conditional branches
        if (INS_IsBranch(ins) && INS_HasFallThrough(ins)){
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) ConditionalBranchAnalysis,
                IARG_INST_PTR, 
                IARG_BRANCH_TAKEN,
                IARG_BRANCH_TARGET_ADDR,
                IARG_END);
        }

        if(INS_IsIndirectControlFlow(ins)) {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
            INS_InsertThenCall(
                ins, 
                IPOINT_BEFORE,
                (AFUNPTR) IndirectBranchPred,
                IARG_INST_PTR,
                IARG_UINT32, INS_Size(ins),
                IARG_BRANCH_TARGET_ADDR,
                IARG_END
            );
        }
        
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_END);
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
        AnalyzeBB(bbl);

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
    
    // Initiliase truth tables
    initTT();
    // initPredictors();

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
