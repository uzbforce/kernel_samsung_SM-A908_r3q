# Changelog

All notable changes to the Raiden Kernel project.

---

## [Raiden v1.0] - 2026-04-24

### Branding
- Renamed kernel from "GrassKernel" to **Raiden Kernel**
- Changed maintainer from "Royna" to **uzbforce**
- Updated localversion from `-ShareMyPerf` to `-Raiden`
- Updated AnyKernel3 installer strings
- Updated build script branding
- Output zip renamed to `RaidenKernel_r3q_YYYY-MM-DD.zip`

### GPU Overclocking
- Adreno 640 overclocked from 600MHz to **800MHz** (+33%)
- Added new GPU frequency steps:
  - 800MHz (TURBO_L1) - new max
  - 750MHz (TURBO)
  - 700MHz (TURBO)
  - 650MHz (TURBO)
  - 600MHz (TURBO) - previous max
- Added bus bandwidth vectors for overclocked frequencies (up to 2400 MB/s)
- Updated GPU power levels (10 active levels + idle)

### I/O Scheduler
- Changed default I/O scheduler from **NOOP** to **BFQ**
- Enabled `CONFIG_IOSCHED_BFQ=y`
- Enabled `CONFIG_BFQ_GROUP_IOSCHED=y` for cgroup-aware scheduling
- BFQ provides better responsiveness and fairness for UFS storage

### Network Optimizations
- Changed default TCP congestion from **Westwood** to **BBR**
- Added `CONFIG_TCP_CONG_BBR=y`
- Added `CONFIG_NET_SCH_FQ_CODEL=y` for low-latency packet scheduling
- Added `CONFIG_NET_SCH_FQ=y` for fair queueing
- BBR provides better throughput and lower latency on modern networks

### CPU Frequency
- Added `CONFIG_CPU_FREQ_GOV_PERFORMANCE=y` governor
- Added `CONFIG_CPU_FREQ_GOV_INTERACTIVE=y` governor
- Set SchedUtil as default governor: `CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL=y`
- Available governors: Performance, Powersave, Userspace, Ondemand, Conservative, SchedUtil, Interactive

### VM & Memory Optimizations
- Enabled `CONFIG_TRANSPARENT_HUGEPAGE=y` with always mode
- Enabled `CONFIG_COMPACTION=y` for memory defragmentation
- Enabled `CONFIG_MIGRATION=y` for page migration
- Enabled `CONFIG_ZSWAP=y` with Zbud and Z3fold compressors
- Enabled `CONFIG_BALLOON_COMPACTION=y`
- Transparent Hugepages reduce TLB pressure for large allocations

### Kernel Configuration
- Set `CONFIG_HZ=300` for better balance between responsiveness and power
- Enabled `CONFIG_CFS_BANDWIDTH=y` for CPU bandwidth control
- Enabled `CONFIG_FAIR_GROUP_SCHED=y`
- Added ChaCha20-Poly1305 crypto for faster encryption
- Added ARM64 AES bit-sliced implementation
- Added AES-CCM support for ARM64 CE
- Enabled `CONFIG_DEBUG_FS=y` and `CONFIG_DYNAMIC_DEBUG=y` for debugging

### Build System
- Rewrote `build_kernel.py` with improvements:
  - Auto-detect CPU cores for parallel builds
  - Added `-j/--jobs` flag for manual thread count
  - Added `--no-lto` option for faster builds
  - Added `--debug` flag for verbose logging
  - Better build environment handling
  - Improved build progress output
  - KCFLAGS optimization flags

### Documentation
- Created comprehensive README.md
- Created this CHANGELOG.md

---

## Technical Details

### SoC: Snapdragon 855 (SM8150)
- CPU: 8 cores (4x Silver + 3x Gold + 1x Gold+)
- GPU: Adreno 640
- Memory: LPDDR4x
- Storage: UFS 2.1/3.0

### Kernel Base
- Version: 4.14.336
- Source: Samsung Galaxy A90 5G (R3Q) stock kernel
- Compiler: LLVM/Clang with LTO

### Root
- KernelSU Next v3.2.0-legacy
- Manual hooks (no GKI)
- SusFS disabled (nosusfs)

---

## Performance Expectations

### GPU Overclock
- Stock: 600MHz max
- Raiden: 800MHz max (+33%)
- Expected GPU performance gain: ~25-30% in GPU-bound workloads

### I/O (BFQ vs NOOP)
- Better I/O fairness between apps
- Reduced I/O latency under load
- Improved foreground app responsiveness

### Network (BBR vs Westwood)
- Better throughput on high-bandwidth networks
- Lower latency and reduced bufferbloat
- Improved performance on lossy networks (mobile data)

### VM/Memory
- Reduced TLB misses with Transparent Hugepages
- Less I/O pressure with Zswap
- Better memory utilization with compaction
