#!/bin/bash -e
cd "$(dirname "$0")"

sources=( \
  src/array.c \
  src/ast.c \
  src/cctx.c \
  src/dlmalloc.c \
  src/hash.c \
  src/memory.c \
  src/os.c \
  src/parse.c \
  src/parseint.c \
  src/ptrmap.c \
  src/resolve_sym.c \
  src/resolve_type.c \
  src/scan.c \
  src/sds.c \
  src/source.c \
  src/str.c \
  src/sym.c \
  src/token.c \
  src/tstyle.c \
  src/typeid.c \
  src/types.c \
  src/unicode.c \
  src/wp.c \
  src/convlit.c \
  \
  src/ir/block.c \
  src/ir/builder.c \
  src/ir/constcache.c \
  src/ir/fun.c \
  src/ir/op.c \
  src/ir/pkg.c \
  src/ir/repr.c \
  src/ir/value.c \
)
# sources=src/*.c

builddir=build
TF=$builddir/.build.ninja
mkdir -p "$builddir"
rm -f "$TF"
touch "$TF"

dev_objects=()
opt_objects=()

for srcfile in ${sources[@]}; do
  objfile=$(basename "$srcfile" .c).o

  opt_objects+=( "$\n  \$builddir/obj/opt__$objfile" )
  dev_objects+=( "$\n  \$builddir/obj/dev__$objfile" )

  echo "build \$builddir/obj/opt__$objfile: compile_obj $srcfile" >> "$TF"
  echo "  cflags = \$cflags_opt" >> "$TF"

  echo "build \$builddir/obj/dev__$objfile: compile_obj $srcfile" >> "$TF"
  echo "  cflags = \$cflags_dev_asan" >> "$TF"

done

echo -e "build \$builddir/wp: link ${opt_objects[@]}" >> "$TF"
echo "  lflags = \$lflags_opt" >> "$TF"

echo -e "build \$builddir/wp.g: link ${dev_objects[@]}" >> "$TF"
echo "  lflags = \$lflags_dev_asan" >> "$TF"

sed -E "/CONFIG_REPLACE_BUILDS/r $TF" "build.in.ninja" \
| sed -E "/CONFIG_REPLACE_BUILDS/d" \
> build.ninja

rm "$TF"
