#!/bin/sh
# Builds the pdp11-aout cross compiler (GNU binutils and GCC, C only, with
# libgcc and no C library) into $PREFIX, default ~/pdp11-tools.  Then:
#
#     export PATH=~/pdp11-tools/bin:$PATH
#     make -C game check
#
# Needs a C/C++ compiler, make, and the GMP, MPFR and MPC libraries
# (Debian/Ubuntu: apt install libgmp-dev libmpfr-dev libmpc-dev texinfo;
# macOS with Homebrew: brew install gmp mpfr libmpc, and pass
# --with-gmp=$(brew --prefix) etc. to GCC's configure if it can't find them).
# The build takes a while: GCC is the long part.
set -e
PREFIX=${PREFIX:-$HOME/pdp11-tools}
BINUTILS=binutils-2.42
GCC=gcc-14.2.0
JOBS=${JOBS:-4}
work=${WORK:-$PWD/pdp11-toolchain-build}

mkdir -p "$work"
cd "$work"
[ -d $BINUTILS ] || curl -L https://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.xz | tar xJ
[ -d $GCC ] || curl -L https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.xz | tar xJ

mkdir -p build-binutils
(cd build-binutils &&
    ../$BINUTILS/configure --target=pdp11-aout --prefix="$PREFIX" \
        --disable-nls --disable-werror --disable-gdb --disable-sim &&
    make -j"$JOBS" && make install)

export PATH="$PREFIX/bin:$PATH"
mkdir -p build-gcc
(cd build-gcc &&
    ../$GCC/configure --target=pdp11-aout --prefix="$PREFIX" \
        --enable-languages=c --without-headers --with-newlib \
        --disable-nls --disable-shared --disable-threads --disable-libssp \
        --disable-libquadmath --disable-libgomp --disable-libatomic \
        --disable-multilib --disable-bootstrap &&
    make -j"$JOBS" all-gcc all-target-libgcc &&
    make install-gcc install-target-libgcc)

echo "installed in $PREFIX/bin"
