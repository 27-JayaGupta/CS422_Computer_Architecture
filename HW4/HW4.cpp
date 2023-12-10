#include "pin.H"
// #include "cache.h"
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iostream>
#include <vector>
#include <list>
#include <utility>
#include <algorithm>
#include <cmath>

using namespace std;

#define KB * 1024
#define MB * 1024 * 1024

#define ONE_BILLION 1e+9

// #define debug(...) printf(__VA_ARGS__)
// #define debug(...)

std::chrono::high_resolution_clock::time_point t_start;
std::chrono::high_resolution_clock::time_point t_end;

UINT64 fastForwardCount;
UINT64 insCount    = 0; //number of dynamically executed instructions

std::ostream* out = &cerr;
string outputFile;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB< INT64 > KnobFastForwardCount(KNOB_MODE_WRITEONCE, "pintool", "f", "", "specify fast forward amount in multiples of 1 billion");

typedef enum {
    LRU = 100,
    SRRIP,
    NRU
} ReplPolicy;

class CacheBlock {
public:
    unsigned int _tag = 0;
    bool _valid = false;
    int _rripValue = -1; // [0,3]
    bool _refBit = 0; // 0 or 1
    unsigned int _hitCount = 0;
    unsigned int _blockAddress = 0;

    void reset() {
        _valid = false;
        _rripValue = -1;
        _refBit = 0;
        _hitCount = 0;
        _blockAddress = 0;
    }

    CacheBlock() {
        _valid = false;
        _hitCount = 0;
        _rripValue = -1;
        _refBit = 0;
        _blockAddress = 0;
    }
};

class Cache {
private:
    int _size;
    int _blockSize;
    int _way;
    int _set;
    int _setBits;
    int _indexMask;
    int _level;
    Cache* _prevCache;
    ReplPolicy _replPolicy;
    vector<list<int>> _lru;
    vector<int> _refBitsSet;

    inline void updateLRU(int setNo, int wayNo) {
        // cout << "Entered LRU updation block\n";
        list<int> &l = _lru[setNo];

        for(auto itr = l.begin(); itr != l.end(); ++itr) {
            if(*itr == wayNo) {
                l.erase(itr);
                l.push_front(wayNo);
                break;
            }
        }
        // cout << "Exited LRU updation block\n";
    }

    inline void updateNRU(int setNo, int wayNo) {
        // cout << "Entered NRU updation block\n";

        if(_cache[setNo][wayNo]._refBit == 0) 
            _refBitsSet[setNo]++;

        if(_refBitsSet[setNo]  == _way) {
            // All ref bits set
            _refBitsSet[setNo] = 1;

            for(int w=0; w<_way; w++){
                _cache[setNo][w]._refBit = 0;
            }
        }
        // cout << "Exited NRU updation block\n";

        _cache[setNo][wayNo]._refBit = 1;
    }

public:
    vector<vector<CacheBlock>> _cache;
    long _numFills = 0;
    long _hit0Count = 0;
    long _hit2Count = 0;
    long _hitat1Count = 0;
    long _hitCount = 0;
    long _missCount = 0;
    long _accessCount = 0;

    Cache(int size, int blockSize, int way, ReplPolicy replPolicy, int level, Cache* prevCache)
    {   
        _size = size;
        _blockSize = blockSize;
        _way = way;
        _replPolicy = replPolicy;
        _level = level;
        _prevCache = prevCache;
        _set = (size)/(blockSize * way);
        _setBits = (int)(log2(_set));
        _indexMask = (_set - 1);

        // cout << "level: " << level << " setbits: " << _setBits << " set: " << _set << " way: " << way << " blocksize " << blockSize << endl;
        _lru.resize(_set);
        for(int i=0; i<_set; i++) {
            list<int> l;
            for(int j=0; j<_way; j++) l.push_back(j);
            _lru[i] = l;
            _refBitsSet.push_back(0);
        }

        _cache.resize(_set, vector<CacheBlock>(_way));   
    }

    inline bool lookup(unsigned int address) {
        _accessCount++;
        // cout << "Entered lookup block\n";
        int setNo = address & _indexMask;
        unsigned int tag = (address >> _setBits);
        // cout << _replPolicy << " Lookup: addr: " << address << " , level: " << _level <<  ", setNo: " << setNo << ", tag: " << tag << "\n";

        for(int w=0; w<_way; w++) {
            CacheBlock &block = _cache[setNo][w];
            if(block._valid) {
                if(block._tag == tag) {
                    _hitCount ++;
                    block._hitCount++;
                    switch (_replPolicy)
                    {
                    case LRU:
                        updateLRU(setNo, w);
                        break;
                    case SRRIP:
                        block._rripValue = 0;
                        break;
                    case NRU:
                        updateNRU(setNo, w);
                        break;
                    default:
                        cerr << "Unknown Repl Policy\n";
                        exit(-1);
                        break;
                    }

                    // cout << "Hit in L" << _level << "\n";
                    return true;
                }
            }
        }
        // cout << "Exited LRU updation block, Miss in L<< " << _level << " \n";

        ++_missCount;
        return false;
    }

    inline void fill(unsigned int address) {
        _numFills++;
        // cout << "Entered Fill block\n";
        int setNo = address & _indexMask;
        unsigned int tag = (address >> _setBits);
        bool emptyBlock = false;
        int emptyWay = -1;

        for(int w=0; w<_way; w++) {
            CacheBlock &block = _cache[setNo][w];
            
            if(!block._valid) {
                // Found an empty block
                emptyBlock = true;
                emptyWay = w;
                break;
            }
        }

        if(!emptyBlock) {
            // No empty block found
            pair<int, unsigned int> p = evict(setNo);
            emptyWay = p.first;
            if(_level == 2) {
                // L2 cache
                _prevCache->invalidate(p.second);
            }
        }

        // Create a new entry
        CacheBlock &block = _cache[setNo][emptyWay];
        block._hitCount = 0;
        block._tag = tag;
        block._valid = true;
        block._blockAddress = address;

        switch (_replPolicy)
        {
        case LRU:
            updateLRU(setNo, emptyWay);
            break;
        case SRRIP:
            block._rripValue = 2;
            break;
        case NRU:
            updateNRU(setNo, emptyWay);
            break;
        default:
            cerr << "Unknown Repl Policy\n";
            exit(-1);
            break;
        }

        if(_level == 2) {
            _prevCache->fill(address);
        }

        // cout << "Exited Fill block\n";
    }

    inline void invalidate(unsigned int address) {
        // cout << "Entered invalidate block\n";

        int setNo = address & _indexMask;
        unsigned int tag = (address >> _setBits);

        for(int w=0; w<_way; w++) {
            CacheBlock &block = _cache[setNo][w];
            if(block._valid) {
                if(block._tag == tag) {
                    block.reset();
                    if(_replPolicy == ReplPolicy::NRU){
                        _refBitsSet[setNo]--;
                    }
                }
            }
        }

        //  cout << "Exited invalidate block\n";

    }

    inline pair<int, unsigned int> evict(int setNo) {
        // cout << "Entered evict block\n";
        int wayNo = -1;
        int maxRRIP = -1;
        int diff;
        switch (_replPolicy)
        {
        case LRU:
            wayNo = _lru[setNo].back();
            break;
        case SRRIP:
            maxRRIP = -1;
            for(int w=0; w<_way; w++) {
                if(_cache[setNo][w]._rripValue > maxRRIP) {
                    maxRRIP = _cache[setNo][w]._rripValue;
                    wayNo = w;
                    if(maxRRIP == 3)
                        break;
                }
            }

            diff = 3 - maxRRIP;
            if(diff) {
                for(CacheBlock &b: _cache[setNo]) {
                    b._rripValue += diff;
                }
            }
            break;
        case NRU:
            for(int w=0; w<_way; w++) {
                if(_cache[setNo][w]._refBit == 0) {
                    wayNo = w;
                    break;
                }
            }

            break;
        default:
            cerr << "Unknown Repl Policy\n";
            exit(-1);
            break;
        }

        if(wayNo == -1) {
            cerr <<"No suitable block found for eviction. Eviction Policy: " << _replPolicy << "\n";
            exit(-1);
        }

        CacheBlock &block = _cache[setNo][wayNo];
        pair<int, unsigned int> p = make_pair(wayNo, block._blockAddress);
        if(block._hitCount == 0) _hit0Count++;
        if(block._hitCount >= 2) _hit2Count++;
        if(block._hitCount >= 1) _hitat1Count++;
        block.reset();

        // cout << "Exited evict block\n";
        return p;
    }
};

Cache* L1_LRU_1;
Cache* L2_LRU;
Cache* L1_LRU_2;
Cache* L2_SRRIP;
Cache* L1_LRU_3;
Cache* L2_NRU;

void simulate_caches(unsigned int address, Cache* L1, Cache* L2) {
    // printf("Lookup L1 addr: %#x\n",address);
    if(!(L1->lookup(address))){
        // cout << "L1 Lookup failed: " << std::hex << address << '\n';
        if(L2->lookup(address)) {
            // cout << "L2 lookup success: " << std::hex << address << '\n';
            L1->fill(address);
        }
        else {
            // cout << "L2 lookup failed: " << std::hex << address << '\n';
            L2->fill(address);
        }
    }
    // cout << "done: " << std::hex << address << endl;
}

// Print a memory read record
VOID RecordMem(VOID * addr, UINT32 size)
{   
    // cout << "PINTOOL: address: " << addr << ", Size: " << size << std::endl;
    unsigned int addrint = (unsigned int) addr;
    // cout << "PINTOOL: address: " << std::hex << addrint << ", Size: " << size << std::endl;
    unsigned int startAddr = (addrint >> 6);
    unsigned int endAddr = ((addrint + size - 1) >> 6);

    for(unsigned int address = startAddr; address <= endAddr; address++){
        simulate_caches(address, L1_LRU_1, L2_LRU);
        simulate_caches(address, L1_LRU_2, L2_SRRIP);
        simulate_caches(address, L1_LRU_3, L2_NRU);
    }
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

template<typename T> 
inline void printElement(T t)
{
    *out << left << setw(30) << t;
}

VOID MyExitRoutine(void) {
    
    // Print stats here
    printElement("DESIGN1: L1(LRU) and L2(LRU)"); *out<<endl;
    printElement("L1 Access: "); printElement(L1_LRU_1->_accessCount); *out<<endl;
    printElement("L2 Access: "); printElement(L2_LRU->_accessCount); *out<<endl;
    printElement("L1 Misses: "); printElement(L1_LRU_1->_missCount); *out<<endl;
    printElement("L2 Misses: "); printElement(L2_LRU->_missCount); *out<<endl;
    printElement("L2 dead-on-fill Blocks: "); printElement((((double)L2_LRU->_hit0Count)/L2_LRU->_numFills) * 100.0); *out<<endl;
    printElement("L2 (atleast 2 hits/ atleast 1 hit): "); 
    if(L2_LRU->_hitat1Count) printElement((L2_LRU->_hit2Count*100.00)/L2_LRU->_hitat1Count);
    else printElement("0.0");
    *out<<endl;
    *out<<endl;
    printElement("L2(atleast 2 hits: )"); printElement(L2_LRU->_hit2Count); *out<<endl;
    printElement("L2(atleast 1 hits: )"); printElement(L2_LRU->_hitat1Count); *out<<endl;

    *out<<endl;
    *out<<endl;
    printElement("DESIGN2: L1(LRU) and L2(SRRIP)"); *out<<endl;
    printElement("L1 Misses: "); printElement(L1_LRU_2->_missCount); *out<<endl;
    printElement("L2 Misses: "); printElement(L2_SRRIP->_missCount); *out<<endl;

    *out<<endl;
    *out<<endl;

    printElement("DESIGN3: L1(LRU) and L2(NRU)"); *out<<endl;
    printElement("L1 Misses: "); printElement(L1_LRU_3->_missCount); *out<<endl;
    printElement("L2 Misses: "); printElement(L2_NRU->_missCount); *out<<endl;

    *out<<endl;
    *out<<endl;
    printElement("-----------------------------------------------------------------------------------------------------------"); *out << endl;

    t_end = std::chrono::high_resolution_clock::now();

    auto elapsed_time_s = std::chrono::duration_cast<std::chrono::seconds>(t_end-t_start).count();
    
    *out << "Total ins instrumented: " << insCount << endl;
    *out << "Total Execution Time: "  << elapsed_time_s << endl;
    exit(0);
}

VOID CountIns()
{
    insCount ++;
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{   
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) Terminate, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) MyExitRoutine, IARG_END);

    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {   
            // FastForward() is called for every ins executed
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT32, INS_MemoryOperandSize(ins, memOp),
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {   
            // FastForward() is called for every ins executed
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT32, INS_MemoryOperandSize(ins, memOp),
                IARG_END);
        }
    }

    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_END);
}

VOID Fini(INT32 code, VOID *v)
{   
    cout << "Called from FINI\n";
    MyExitRoutine();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    cerr << "This Pintool simulates L1 and L2 caches\n";
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    outputFile = KnobOutputFile.Value();

    cout << "outputFile: " << outputFile << endl;
    if (!outputFile.empty())
    {
        out = new std::ofstream(outputFile.c_str());
    }

    fastForwardCount = KnobFastForwardCount.Value();
    fastForwardCount *= ONE_BILLION;
    cout << "FF count: " << fastForwardCount << endl;
    
    L1_LRU_1 = new Cache(64 KB, 64, 8, ReplPolicy::LRU, 1, NULL);
    L2_LRU = new Cache(1 MB, 64, 16, ReplPolicy::LRU, 2, L1_LRU_1);

    L1_LRU_2 = new Cache(64 KB, 64, 8, ReplPolicy::LRU, 1, NULL);
    L2_SRRIP = new Cache(1 MB, 64, 16, ReplPolicy::SRRIP, 2, L1_LRU_2);

    L1_LRU_3 = new Cache(64 KB, 64, 8, ReplPolicy::LRU, 1, NULL);
    L2_NRU = new Cache(1 MB, 64, 16, ReplPolicy::NRU, 2, L1_LRU_3);

    // Register function to be called to simulate caches
    INS_AddInstrumentFunction(Instruction, 0);

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