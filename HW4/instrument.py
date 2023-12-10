import sys
import os
import subprocess
import shutil

if (len(sys.argv) == 1):
    print(["400.perlbench", "403.gcc", "429.mcf", "436.cactusADM", "437.leslie3d", "450.soplex", "456.hmmer", "462.libquantum", "470.lbm", "471.omnetpp", "482.sphinx3", "483.xalancbmk", "401.bzip2"])
    exit(1)

config = {
    "spec_dir":"/home/jaya/pin-3.28-98749-g6643ecee5-gcc-linux/source/tools/CS422_Computer_Architecture/HW4/spec_2006",
    "pin_exec": "/home/jaya/pin-3.28-98749-g6643ecee5-gcc-linux/pin",
    "pintool": "/home/jaya/pin-3.28-98749-g6643ecee5-gcc-linux/source/tools/CS422_Computer_Architecture/HW4/obj-ia32/HW4_32.so",
    "hw_dir": "/home/jaya/pin-3.28-98749-g6643ecee5-gcc-linux/source/tools/CS422_Computer_Architecture/HW4"
}

benchmarks = ["400.perlbench", "403.gcc", "429.mcf", "436.cactusADM", "437.leslie3d", "450.soplex", "456.hmmer", "462.libquantum", "470.lbm", "471.omnetpp", "482.sphinx3", "483.xalancbmk", "401.bzip2"]

props = {}

def make_props(bm:str, ff:int, cmd: str) -> None:
    props[bm] = {"ff": ff, "cmd": cmd}

def gen_benchmark_props():
    make_props(benchmarks[0], 207, "-- ./perlbench_base.i386 -I./lib diffmail.pl 4 800 10 17 19 300 > perlbench.ref.diffmail.out 2> perlbench.ref.diffmail.err")
    make_props(benchmarks[1], 107, "-- ./gcc_base.i386 cp-decl.i -o cp-decl.s > gcc.ref.cp-decl.out 2> gcc.ref.cp-decl.err")
    make_props(benchmarks[2], 377, "-- ./mcf_base.i386 inp.in > mcf.ref.out 2> mcf.ref.err")
    make_props(benchmarks[3], 584, "-- ./cactusADM_base.i386 benchADM.par > cactusADM.ref.out 2> cactusADM.ref.err")
    make_props(benchmarks[4], 2346, "-- ./leslie3d_base.i386 < leslie3d.in > leslie3d.ref.out 2> leslie3d.ref.err")
    make_props(benchmarks[5], 364, "-- ./soplex_base.i386 -m3500 ref.mps > soplex.ref.ref.out 2> soplex.ref.ref.err")
    make_props(benchmarks[6], 264, "-- ./hmmer_base.i386 nph3.hmm swiss41 > hmmer.ref.nph3.out 2> hmmer.ref.nph3.err")
    make_props(benchmarks[7], 3605, "-- ./libquantum_base.i386 1397 8 > libquantum.ref.out 2> libquantum.ref.err")
    make_props(benchmarks[9], 43, "-- ./omnetpp_base.i386 omnetpp.ini > omnetpp.ref.log 2> omnetpp.ref.err")
    make_props(benchmarks[11], 1331, "-- ./xalancbmk_base.i386 -v t5.xml xalanc.xsl > xalancbmk.ref.out 2> xalancbmk.ref.err")
    make_props(benchmarks[12], 301, "-- ./bzip2_base.i386 input.source 280 > bzip2.ref.source.out 2> bzip2.ref.source.err")


procs = []

def instrument_benchmark(bm: str):
    os.chdir(config["spec_dir"] + "/"+bm)
    print("Instrumenting " + bm + "...")
    cmd = "time " + config["pin_exec"]+ " -t " + config["pintool"] + " -f " + str(props[bm]["ff"]) + " -o " + bm + ".out " + props[bm]["cmd"]
    # cmd = "time " + config["pin_exec"]+ " -t " + config["pintool"] + " -f 0 -o ls.out -- /bin/ls" 
    procs.append(subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True, shell=True))

def recompile_pintool():
    print("Compiling pintool...")
    os.chdir(config["hw_dir"])
    try:
        shutil.rmtree(config["hw_dir"]+"/obj-ia32")
    except:
        pass
    res = subprocess.run(["make TARGET=ia32 obj-ia32/HW4_32.so"], text=True,shell=True)
    if res.returncode != 0:
        print("compilation failed")
        exit(1)
    
recompile_pintool()
gen_benchmark_props()

for i in range(1, len(sys.argv)):
    if not sys.argv[i] in benchmarks:
        continue 
    instrument_benchmark(sys.argv[i])
    
for p in procs:
    p.wait()
