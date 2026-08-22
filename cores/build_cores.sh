#!/usr/bin/env bash
# Fetches and builds the libretro cores this launcher links against.
#
# The cores are large third-party checkouts, so they are not vendored; this
# script reproduces them from scratch, including the local port fixes kept in
# cores/patches.
set -euo pipefail

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PORTLIBS_PATH="${PORTLIBS_PATH:-$DEVKITPRO/portlibs}"
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

fetch() {
    local dir="$1" url="$2"
    if [[ ! -d "$HERE/$dir" ]]; then
        echo "==> cloning $dir"
        git clone --depth 1 "$url" "$HERE/$dir"
    fi
}

apply_patch() {
    local dir="$1" patch="$2"
    [[ -f "$HERE/patches/$patch" ]] || return 0
    # Skip if it is already in the tree.
    if git -C "$HERE/$dir" apply --reverse --check "$HERE/patches/$patch" >/dev/null 2>&1; then
        echo "==> $patch already applied"
        return 0
    fi
    echo "==> applying $patch"
    git -C "$HERE/$dir" apply "$HERE/patches/$patch"
}

# --- mupen64plus-next -------------------------------------------------------
# Provides GLideN64, Angrylion and ParaLLEl-RDP on Switch. ParaLLEl-RDP needs a
# Vulkan driver; on Horizon there is no loader to dlopen, so the patches below
# resolve the ICD statically against nxvk's NVK.
fetch mupen64plus-next https://github.com/libretro/mupen64plus-libretro-nx.git
apply_patch mupen64plus-next mupen64plus-next-libnx-virtmem.patch
apply_patch mupen64plus-next mupen64plus-next-libnx-vulkan-static.patch

# HAVE_PARALLEL_RSP stays off: its JIT allocator wants POSIX mmap with
# separate RW/RX commits, which on Horizon needs libnx's W^X Jit API. The RDP
# is what gives the pixel-accurate rendering, and it works without the RSP JIT.
echo "==> building mupen64plus-next for libnx (ParaLLEl-RDP enabled)"
make -C "$HERE/mupen64plus-next" platform=libnx HAVE_PARALLEL_RDP=1 HAVE_PARALLEL_RSP=0 -j"$JOBS"

echo
echo "Built:"
ls -la "$HERE/mupen64plus-next"/*.a
