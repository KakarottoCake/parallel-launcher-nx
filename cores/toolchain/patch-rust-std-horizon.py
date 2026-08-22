#!/usr/bin/env python3
"""Fix a std bug that blocks building for newlib targets without AT_FDCWD.

set_perm_nofollow()'s cfg_select! excludes espidf/horizon from the Linux arm,
but its fallback arm still calls fchmodat(AT_FDCWD, ...). libc defines
AT_FDCWD for newlib only on vita and rtems, so std fails to compile for
aarch64-switch-horizon. Neither target has symlinks, so plain chmod is the
correct equivalent -- which is what the function's own comment implies was
intended.
"""
import glob
import sys

SENTINEL = 'any(target_os = "espidf", target_os = "horizon") =>'

NEEDLE = '''        _ => {
            cvt_r(|| unsafe {
                libc::fchmodat(libc::AT_FDCWD, p.as_ptr(), perm.mode, 0)
            })
            .map(|_| ())
        }
'''

REPLACEMENT = '''        any(target_os = "espidf", target_os = "horizon") => {
            // These targets have no fchmodat and no symlinks, so chmod is
            // exactly equivalent to a nofollow chmod here.
            cvt_r(|| unsafe { libc::chmod(p.as_ptr(), perm.mode) }).map(|_| ())
        },
        _ => {
            cvt_r(|| unsafe {
                libc::fchmodat(libc::AT_FDCWD, p.as_ptr(), perm.mode, 0)
            })
            .map(|_| ())
        }
'''

paths = glob.glob(
    "/opt/rust/rustup/toolchains/*/lib/rustlib/src/rust/library/std/src/sys/fs/unix.rs")
if not paths:
    sys.exit("no rust-src std found; is the rust-src component installed?")

for path in paths:
    text = open(path).read()
    # Sentinel must be text this script alone introduces. Checking for
    # 'horizon' or 'libc::chmod' alone false-matches: both already appear in
    # this function's neighbourhood.
    if SENTINEL in text:
        print(f"already patched: {path}")
        continue
    if NEEDLE not in text:
        sys.exit(f"could not find the fallback arm in {path}; std has changed shape")
    open(path, "w").write(text.replace(NEEDLE, REPLACEMENT, 1))
    print(f"patched: {path}")
