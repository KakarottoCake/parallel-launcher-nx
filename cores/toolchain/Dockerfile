# nxvk's toolchain image installs SPIRV-Tools from Debian apt, which is 2023.1.1.
# Mesa 26.1 requires >= 2024.1 and ships no subproject wrap for it, so the native
# tools build dies at configure time. Layer a new enough SPIRV-Tools into
# /usr/local, which pkg-config searches ahead of /usr.
FROM nvk-switch-build

ARG SPIRV_TAG=v2025.3

RUN set -eux; \
    git clone --depth 1 -b "${SPIRV_TAG}" \
        https://github.com/KhronosGroup/SPIRV-Tools.git /tmp/SPIRV-Tools; \
    cd /tmp/SPIRV-Tools; \
    # Pulls the exact SPIRV-Headers revision this release was tested against,
    # rather than us guessing a compatible tag pair.
    python3 utils/git-sync-deps; \
    cmake -S /tmp/SPIRV-Tools -B /tmp/SPIRV-Tools/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DSPIRV_SKIP_TESTS=ON \
        -DSPIRV_WERROR=OFF \
        -DBUILD_SHARED_LIBS=OFF; \
    ninja -C /tmp/SPIRV-Tools/build install; \
    rm -rf /tmp/SPIRV-Tools

# Fail loudly here rather than deep inside a Mesa configure run.
RUN test "$(pkg-config --modversion SPIRV-Tools)" != "2023.1.1" \
    && pkg-config --modversion SPIRV-Tools

# Rust std cannot currently be built for aarch64-switch-horizon: its
# set_perm_nofollow() falls through to fchmodat(AT_FDCWD, ...) on every target
# that is not Linux, but libc only defines AT_FDCWD for the vita and rtems
# newlib targets. Patch rust-src before build-std runs.
COPY patch-rust-std-horizon.py /usr/local/bin/patch-rust-std-horizon.py
RUN python3 /usr/local/bin/patch-rust-std-horizon.py
