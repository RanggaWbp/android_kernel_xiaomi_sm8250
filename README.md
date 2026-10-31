# Xiaomi SM8250 Kernel

## Warning

The kernel source code is still under development and may cause some unpredictable problems. Please use it with caution.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Supported Devices](#supported-devices)
- [Build Instructions](#build-instructions)
  - [Quick Build](#quick-build)
  - [Manual Build](#manual-build)

---

## Introduction

This repository is based on [LineageOS/android_kernel_qcom_sm8250](https://github.com/LineageOS/android_kernel_qcom_sm8250), incorporating [Strawing's repository](https://github.com/liyafe1997/kernel_xiaomi_sm8250_mod) and [UtsavBalar1231's repository](https://github.com/UtsavBalar1231/kernel_xiaomi_sm8250), as well as the original [LineageOS/android_kernel_xiaomi_sm8250](https://github.com/LineageOS/android_kernel_xiaomi_sm8250). Some commits also originate from other developers in the open-source community.

---

## Features

This kernel supports [BakaSU](https://github.com/Baka-SU/BakaSU) & [SuSFS](https://gitlab.com/simonpunk/susfs4ksu) & [NoMount](https://github.com/maxsteeel/nomount) & [BORE](https://github.com/firelzrd/bore-scheduler) (Burst-Oriented Response Enhancer) scheduler.  
Please install the BakaSU Manager by yourself.  
The NoKernelSU version supports Magisk and APatch (and their forks).  
Droidspaces (LXC/Docker container support) can be enabled as an optional build flag.  
BORE is always enabled (`CONFIG_SCHED_BORE=y`) to improve UI responsiveness under heavy background load.
|NTSYNC (emulation of Windows NT synchronization primitives, e.g. for Wine/Proton) is enabled as a module, and BBRv3 is the default TCP congestion control (`CONFIG_DEFAULT_TCP_CONG="bbr3"`). The [POC](https://github.com/firelzrd/poc-selector) (Piece-Of-Cake) fast idle-CPU selector is enabled (`CONFIG_SCHED_POC_SELECTOR=y`), the [reflex](https://github.com/firelzrd/reflex) cpufreq governor is built-in (`CONFIG_CPU_FREQ_GOV_REFLEX=y`), and the [cambyses](https://github.com/firelzrd/cambyses) CFS load-balancer modification is applied.

The prebuilt kernel in the **Release** section is compiled from the `android17-aptusitu` branch, and should work on stock MIUI/HyperOS as well as third-party AOSP-based ROMs for Android 11–17.  
Feedback is welcome (via Issues or Pull Requests)! Coolapk users can join the discussion in [this post](https://www.coolapk.com/feed/69700122), you can also join my QQ group, or send me private feedback.

Below are some of the key features:

1. F2FS with realtime discard enabled for improved flash TRIM behavior
2. Support for EROFS
3. zRAM with support for multiple compression algorithms, including LZO, LZ4, LZ4HC, and ZSTD, enabled ZRAM_WRITEBACK, upgraded LZ4 and ZSTD
4. Backported BPF from Linux 5.15 and clone3 (Android 16/17 compatible)
5. Introduced [LE9EC](https://github.com/hakavlad/le9-patch) to optimize memory
6. Backported Binder from 5.10; MIUI builds incorporate millet from xaga, while AOSP builds incorporate [Re:Kernel](https://github.com/Sakion-Team/Re-Kernel)
7. Fixes [the issue where the battery percentage gets stuck at 1%](https://github.com/liyafe1997/Xiaomi-fix-battery-one-percent), and supports recognizing higher-capacity replacement batteries
8. Integrate [BBG(Baseband-guard)](https://github.com/vc-teahouse/Baseband-guard)
9. [BORE](https://github.com/firelzrd/bore-scheduler) scheduler for burst-based task prioritization
10. NTSYNC: emulation of Windows NT synchronization primitives (e.g. for Wine/Proton) as a module
|11. BBRv3 TCP congestion control, set as the default (`CONFIG_DEFAULT_TCP_CONG="bbr3"`)
|12. [POC](https://github.com/firelzrd/poc-selector) (Piece-Of-Cake) fast idle-CPU selector, all static keys enabled by default
|13. [reflex](https://github.com/firelzrd/reflex) cpufreq governor (v0.4.0), built-in (`CONFIG_CPU_FREQ_GOV_REFLEX=y`)
|14. [cambyses](https://github.com/firelzrd/cambyses) CFS load-balancer modification (v0.6.0), built-in

---

## Supported Devices

| Codename | Device Name |
|----------|-------------|
| psyche   | Xiaomi 12X |
| thyme    | Xiaomi 10S |
| umi     | Xiaomi 10 |
| munch   | Redmi K40S / POCO F4 |
| lmi     | Redmi K30 Pro / POCO F2 Pro |
| cmi     | Xiaomi 10 Pro |
| cas     | Xiaomi 10 Ultra |
| apollo  | Xiaomi 10T / Redmi K30S Ultra |
| alioth  | Xiaomi 11X / POCO F3 / Redmi K40 |
| elish   | Xiaomi Pad 5 Pro |
| enuma   | Xiaomi Pad 5 Pro 5G |
| dagu    | Xiaomi Pad 5 Pro 12.4 |
| pipa    | Xiaomi Pad 6 |

---

## Build Instructions

### Quick Build

1. Fork this repo (don’t forget to leave a Star~)
2. Go to **Actions**
3. If you want to compile the kernel for all supported devices, find `Build All Devices Kernel (Matrix Parallel + Release)` and click `Run workflow`
4. If you want to compile the kernel for a single device, find `Build Kernel`, click `Run workflow`, and select the necessary options

---

### Manual Build

1. Prepare the build environment.
   You need `git`, `make`, `curl`, `bison`, `flex`, `zip`, etc.
   - On Debian/Ubuntu:
   ```
   sudo apt install build-essential git curl wget bison flex zip bc cpio libssl-dev ccache tar
   ```
   You also need `python` (not just `python3`):
   ```
   sudo apt install python-is-python3
   ```

   - On RHEL/RPM-based OS:
   ```
   sudo yum groupinstall 'Development Tools'
   sudo yum install wget bc openssl-devel ccache tar
   ```

   Note: `ccache` is enabled in `build.sh` (`$HOME/.cache/ccache_mikernel`). You may remove/modify it.

2. Download [ZyC-Clang v16](https://github.com/ZyCromerZ/Clang/releases/tag/16.0.6-20260807-release) toolchain:
   ```
   mkdir zyc-clang
   cd zyc-clang
   wget https://github.com/ZyCromerZ/Clang/releases/download/16.0.6-20260807-release/Clang-16.0.6-20260807.tar.gz
   tar -zxvf Clang-16.0.6-20260807.tar.gz
   cd ..
   ```

3. Build:
   - Without KernelSU:
     ```
     bash build_kernel.sh TARGET_DEVICE TARGET_OS(omit to build for all)
     ```
   - With KernelSU (BakaSU + SuSFS + NoMount):
     ```
     bash build_kernel.sh TARGET_DEVICE ksu TARGET_OS(omit to build for all)
     ```
   - With Droidspaces (LXC/Docker support):
     Requires `coccinelle` (spatch) to be installed.
     ```
     bash build_kernel.sh TARGET_DEVICE droidspaces TARGET_OS(omit to build for all)
     ```

   Example:
   - lmi (Redmi K30 Pro/POCO F2 Pro) without KernelSU, compile for MIUI:
     ```
     bash build_kernel.sh lmi miui
     ```
   - cmi (Xiaomi 10 Pro) with KernelSU, compile for MIUI:
     ```
     bash build_kernel.sh cmi ksu miui
     ```
   - umi (Xiaomi 10) with KernelSU, compile for AOSP and MIUI:
     ```
     bash build_kernel.sh umi ksu
     ```

   Additionally, `buildall.sh` can build for all supported devices at once.
