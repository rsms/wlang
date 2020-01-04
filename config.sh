#!/bin/bash -e
cd "$(dirname "$0")"

sources=( \
  src/array.c \
  src/ast.c \
  src/hash.c \
  src/parse.c \
  src/parseint.c \
  src/ptrmap.c \
  src/resolve.c \
  src/scan.c \
  src/sds.c \
  src/source.c \
  src/sym.c \
  src/unicode.c \
  src/util.c \
  src/wp.c \
)
# sources=src/*.c

dev_objects=()
opt_objects=()

builddir=build
TF=$builddir/.build.ninja
mkdir -p "$builddir"
rm -f "$TF"
touch "$TF"

for srcfile in ${sources[@]}; do
  objfile=$(basename "$srcfile" .c).o

  dev_objects+=( "$\n  \$builddir/obj/dev__$objfile" )
  opt_objects+=( "$\n  \$builddir/obj/opt__$objfile" )

  echo "build \$builddir/obj/dev__$objfile: compile_obj $srcfile" >> "$TF"
  echo "  cflags = \$cflags_dev" >> "$TF"
  echo "build \$builddir/obj/opt__$objfile: compile_obj $srcfile" >> "$TF"
  echo "  cflags = \$cflags_opt" >> "$TF"
done

echo -e "build \$builddir/wp.g: link ${dev_objects[@]}" >> "$TF"
echo "  lflags = \$lflags_dev" >> "$TF"
echo -e "build \$builddir/wp: link ${opt_objects[@]}" >> "$TF"
echo "  lflags = \$lflags_opt" >> "$TF"

sed -E "/CONFIG_REPLACE_BUILDS/r $TF" "build.in.ninja" \
| sed -E "/CONFIG_REPLACE_BUILDS/d" \
> build.ninja

rm "$TF"
