# NVMain 2.0 (Architectural Simulator) Domain Knowledge
[cite_start]**Context**: Cycle-accurate architectural main memory simulation[cite: 151]. 

## 🛠️ APPLIED REPAIRS (DO NOT REVERT OR FLAG AS BUGS)
1. [cite_start]**Memory Deallocation Fix**: In `FlipNWrite.cpp` (line 265), the code correctly uses `delete modifyCount;` instead of `delete [] modifyCount;` to fix a fatal array deallocation mismatch[cite: 36, 168]. Do not change this back.
2. [cite_start]**Build System**: SCons has been modernized via `2to3`[cite: 36]. [cite_start]SConstruct contains `CCFLAGS='-Wno-error=deprecated-copy'`[cite: 36, 167]. 
3. **Negative Activate-Energy Clamp**: In `src/SubArray.cpp`'s `Activate()`, the DRAM-model energy formula `(EIDD0*tRC) - (EIDD3N*tRAS + EIDD2N*tRP)` assumes `EIDD0` (one-bank activate-precharge current) exceeds the `tRAS`/`tRP`-weighted background current. Real hynix/Micron DDR5-4800 datasheet values invert this (e.g. hynix `EIDD0=60.2 < EIDD3N=117.6`), which yields a negative incremental activate energy — physically impossible. The per-activate delta (`actDelta`) is clamped at `0.0` before being added to `subArrayEnergy`/`activeEnergy`. Documented in `results/cycle6c_ddr5_calibration_and_provenance_report.md` and MBMM_Book_Typst's Project_Book.typ §3.1.6 item (8). Do not revert.

## 🚀 SIMULATION PIPELINE
[cite_start]We **DO NOT** use the integrated gem5 C++ wrapper due to port-binding collisions[cite: 131, 132]. [cite_start]We use a decoupled, trace-based simulation pipeline[cite: 132, 133]. [cite_start]Traces are parsed from SCALE-Sim or gem5 and padded with 128-byte dummy strings to satisfy NVMain's strict constraints[cite: 134, 655].