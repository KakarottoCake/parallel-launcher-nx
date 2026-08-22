#!/usr/bin/env bash
# Builds the nxvk (Mesa/NVK + Zink) static archives for the Switch.
#
# The upstream README's command sequence does not work against current Rust and
# cargo. This captures the corrected order plus the fixes, all of which live in
# cores/toolchain/ and cores/patches/ rather than as ad-hoc shell state:
#
#   1. toolchain image, plus a layer adding SPIRV-Tools >= 2024.1 and patching
#      rust-src so std can compile for a newlib target
#   2. the Rust sysroot, which the README omits entirely
#   3. native mesa_clc / vtn_bindgen2
#   4. clang resource dir beside the install prefix, so mesa_clc can find
#      opencl-c-base.h
#   5. configure + build the cross-zink superset (NVK for Vulkan, Zink for GL)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NXVK="$HERE/nxvk"
IMAGE=nvk-switch-build-spirv
BUILD=switch/build/cross-zink

command -v docker >/dev/null || { echo "docker is required" >&2; exit 1; }
docker info >/dev/null 2>&1 || { echo "the docker daemon is not running" >&2; exit 1; }

[[ -d "$NXVK" ]] || git clone --depth 1 https://github.com/PalindromicBreadLoaf/nxvk.git "$NXVK"

# Local fix: cargo moved where -Zbuild-std leaves its rlibs.
if ! git -C "$NXVK" apply --reverse --check "$HERE/patches/nxvk-sysroot-rlib-paths.patch" >/dev/null 2>&1; then
    git -C "$NXVK" apply "$HERE/patches/nxvk-sysroot-rlib-paths.patch"
fi

docker image inspect nvk-switch-build >/dev/null 2>&1 \
    || docker build -t nvk-switch-build "$NXVK/switch/docker"
docker image inspect "$IMAGE" >/dev/null 2>&1 \
    || docker build -t "$IMAGE" "$HERE/toolchain"

docker run --rm -v "$NXVK:/work" -w /work "$IMAGE" bash -euo pipefail -c '
    if [ ! -f switch/rust/sysroot/lib/rustlib/aarch64-switch-horizon/lib/libstd-*.rlib ] 2>/dev/null; then
        echo "==> rust sysroot"
        bash switch/rust/build-std-sysroot.sh
    fi

    if [ ! -x switch/build/native-tools/bin/mesa_clc ]; then
        echo "==> native tools"
        bash switch/build/build-native-tools.sh
    fi

    # clang locates its resource headers relative to argv[0]; mesa_clc is
    # installed to a prefix that has no lib/clang, so opencl-c-base.h goes
    # missing. Point the expected path at the system clang.
    CLANG_RES=$(dirname "$(find /usr/lib/llvm-*/lib/clang -maxdepth 1 -mindepth 1 -type d | head -1)")
    CLANG_VER=$(basename "$(find /usr/lib/llvm-*/lib/clang -maxdepth 1 -mindepth 1 -type d | head -1)")
    mkdir -p switch/build/native-tools/lib/clang
    ln -sfn "$CLANG_RES/$CLANG_VER" "switch/build/native-tools/lib/clang/$CLANG_VER"
    ln -sfn "$CLANG_RES/$CLANG_VER" "switch/build/native-tools/lib/clang/${CLANG_VER%%.*}"

    export PATH=/work/switch/build/native-tools/bin:$PATH

    echo "==> configure cross-zink"
    bash switch/build/configure-zink.sh

    # Two things to know about this step:
    #  - The final libvulkan_nouveau.so link always fails on Horizon: a shared
    #    object cannot be linked against libnx, whose crt0 also defines _start.
    #    Upstream documents this. The static archives are what we consume.
    #  - The GL/Zink archives are not dependencies of the Vulkan target, so
    #    asking only for the .so silently leaves them unbuilt. Name them all.
    echo "==> build archives"
    ninja -k0 -C '"$BUILD"' \
        src/nouveau/vulkan/libvulkan_nouveau.so \
        src/util/libmesa_util.a src/util/libmesa_util_simd.a \
        src/util/blake3/libblake3.a src/c11/impl/libmesa_util_c11.a \
        src/nouveau/compiler/libnak.a src/nouveau/compiler/libnak_rs.a \
        src/compiler/rust/libcompiler_c_helpers.a \
        src/nouveau/headers/libnvidia_headers_c.a \
        src/nouveau/nil/libnil.a src/nouveau/nil/liblibnil_format_table.a \
        src/compiler/nir/libnir.a src/compiler/libcompiler.a \
        src/nouveau/mme/libnouveau_mme.a src/nouveau/winsys/libnouveau_ws.a \
        src/vulkan/util/libvulkan_util.a src/compiler/spirv/libvtn.a \
        src/util/libxmlconfig.a \
        src/egl/libEGL.a src/mesa/libmesa.a \
        src/compiler/glsl/libglsl.a src/compiler/glsl/glcpp/libglcpp.a \
        src/mesa/glapi/shared-glapi/libglapi.a \
        src/mesa/glapi/glapi/libglapi_bridge.a \
        src/gallium/auxiliary/libgallium.a \
        src/gallium/drivers/zink/libzink.a \
        src/gallium/winsys/zink/drm/libzinkwinsys.a \
        src/gallium/winsys/sw/null/libws_null.a \
        src/gallium/winsys/sw/wrapper/libwsw.a || true

    # Meson emits thin archives, which only reference their .o files by a path
    # relative to the build tree. Repack them as real archives into a staging
    # prefix so the launcher build does not have to care where nxvk was built,
    # or keep the build tree alive.
    echo "==> staging fat archives"
    AR=/opt/devkitpro/devkitA64/bin/aarch64-none-elf-ar
    PREFIX=/work/switch/build/prefix
    rm -rf "$PREFIX"; mkdir -p "$PREFIX/lib"
    cd '"$BUILD"'
    for thin in $(find . -name "*.a" -type f); do
        head -c 8 "$thin" | grep -q "thin" || { cp -f "$thin" "$PREFIX/lib/"; continue; }
        members=$($AR t "$thin")
        [ -n "$members" ] || continue
        $AR crs "$PREFIX/lib/$(basename "$thin")" $members
    done
    cd /work
'

PREFIX="$NXVK/switch/build/prefix/lib"
echo
echo "Staged archives in $PREFIX:"
missing=0
for a in libnvk.a libzink.a libEGL.a libmesa.a libgallium.a libnak.a libnir.a \
         libglsl.a libglapi.a libvulkan_util.a libnouveau_ws.a; do
    if [[ -f "$PREFIX/$a" ]]; then
        printf '  OK   %-18s %s\n' "$a" "$(du -h "$PREFIX/$a" | cut -f1)"
    else
        printf '  MISS %s\n' "$a"
        missing=1
    fi
done
echo
echo "total: $(du -sh "$PREFIX" 2>/dev/null | cut -f1)"
exit $missing
