# Raiden Kernel

**Custom kernel for Samsung Galaxy A90 5G (SM-A908/R3Q) with Snapdragon 855 (SM8150)**

---

## About

Raiden Kernel is a performance-focused custom kernel based on Samsung's stock 4.14.336 kernel for the Galaxy A90 5G. It includes comprehensive optimizations for performance, battery life, and responsiveness while maintaining stability.

**Features:**
- Kernel 4.14.336
- SukiSU Ultra (KSU Fork, manual hook)
- AnyKernel3 flashable zip
- Built with LLVM/Clang + LTO

---

## Improvements

### Performance
- **GPU Overclock**: Adreno 640 overclocked from 600MHz to 800MHz (+33%)
  - Additional frequency steps: 650MHz, 700MHz, 750MHz, 800MHz
- **I/O Scheduler**: Changed from NOOP to **BFQ** for better storage responsiveness
- **TCP Congestion**: Changed from Westwood to **BBR** for improved network throughput
- **Network Schedulers**: Added FQ and FQ_CODEL for lower latency
- **CPU Governors**: Added Performance and Interactive governors
- **VM Optimizations**: Transparent Hugepages, Zswap with Z3fold, memory compaction
- **Scheduler**: WALT scheduler with energy-aware scheduling (EAS)
- **Compiler**: LLVM Polly + LTO for optimized code generation

### Battery Life
- Energy-aware scheduling (EAS) enabled by default
- SchedUtil governor as default CPU frequency governor
- Power-efficient workqueues enabled
- CPU idle states optimized
- WQ_POWER_EFFICIENT_DEFAULT enabled
- Zswap enabled for reduced I/O pressure

### Memory
- Transparent Hugepage support (always mode)
- Zswap with Zbud and Z3fold compressors
- Memory compaction and migration enabled
- CMA (Contiguous Memory Allocator) enabled

### Network
- BBR congestion control (default)
- FQ and FQ_CODEL packet schedulers
- WireGuard VPN support built-in
- Full Netfilter/iptables support

### Security
- KernelSU Next 3.2.0 with manual hooks
- SusFS (Suspicious Filesystem) support:
  - Path hiding
  - Mount hiding
  - File open redirection
  - Uname spoofing
  - Symbol hiding
- Hardened usercopy
- Shadow Call Stack
- FORTIFY_SOURCE

---

## Device Support

| Device | Codename | Model |
|--------|----------|-------|
| Samsung Galaxy A90 5G | R3Q | SM-A908 |

---

## Building

### Prerequisites
- Clang/LLVM toolchain (place in `toolchain/` directory)
- Python 3.x
- aarch64-linux-gnu cross-compiler

### Build Commands

```bash
# Standard build (auto-detect CPU cores)
python build_kernel.py

# Build with ThinLTO (faster build, slightly larger binary)
python build_kernel.py --thin

# Build without LTO (fastest build)
python build_kernel.py --no-lto

# Specify parallel jobs
python build_kernel.py -j8

# Allow dirty build (don't clean output directory)
python build_kernel.py --allow-dirty
```

### Output
The build produces a flashable AnyKernel3 zip: `RaidenKernel_r3q_YYYY-MM-DD.zip`

---

## Flashing

1. Reboot to recovery (TWRP recommended)
2. Flash `RaidenKernel_r3q_YYYY-MM-DD.zip`
3. Reboot to system

**Note:** This kernel includes KernelSU Next. Root access will be available after installing the KernelSU manager app.

---

## KernelSU

This kernel includes **KernelSU Next v3.2.0-legacy** with manual hook integration.

- No SusFS (as per build configuration)
- Manual hooks integrated into kernel source
- Compatible with KernelSU Next manager app

---

## Credits

- **uzbforce** - Kernel modifications and optimizations
- **Samsung** - Stock kernel source
- **Qualcomm** - SM8150 platform support
- **KernelSU Next team** - Root solution
- **AnyKernel3 (osm0sis)** - Flashable zip template

---

## Disclaimer

This kernel is provided as-is. Flashing custom kernels may void your warranty and carries inherent risks. Always backup your device before flashing. The authors are not responsible for any damage to your device.

---

## License

This kernel is based on Samsung's Linux kernel source and is licensed under GPL-2.0.
