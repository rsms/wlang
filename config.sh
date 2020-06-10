#!/bin/bash -e
cd "$(dirname "$0")"

sources=( \
  src/array.c \
  src/assert.c \
  src/ast.c \
  src/cctx.c \
  src/convlit.c \
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
  src/sds_test.c \
  src/source.c \
  src/str.c \
  src/sym.c \
  src/test.c \
  src/thread.c \
  src/token.c \
  src/tstyle.c \
  src/typeid.c \
  src/types.c \
  src/unicode.c \
  src/wp.c \
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

# <name>:<executable>
# Each name should have corresponding $cflags_<name> and $lflags_<name> defined in build.in.ninja
products=( \
  opt:wp       \
  dev:wp.g     \
  test:wp.test \
)

builddir=build
TF=$builddir/.build.ninja
mkdir -p "$builddir"
rm -f "$TF"
touch "$TF"

for product in ${products[@]}; do
  TUPLE=(${product//:/ })
  name=${TUPLE[0]}
  exe=${TUPLE[1]}
  objects=()

  echo "# --------------------------------------------------" >> "$TF"
  echo "# ${name} (\$builddir/${exe})" >> "$TF"

  for srcfile in ${sources[@]}; do

    # only include *_test.c files in the "test" target
    if [[ "$srcfile" == *"_test.c" ]] && [[ "$name" != "test" ]]; then
      # echo "skip test $srcfile for target $name"
      continue
    fi

    objfile=$(dirname "$srcfile")/$(basename "$srcfile" .c).o
    objfile=\$builddir/obj/${name}/${objfile//src\//}
    objects+=( "$\n  ${objfile}" )

    echo "build ${objfile}: compile_obj $srcfile" >> "$TF"
    echo "  cflags = \$cflags_${name}" >> "$TF"
  done

  echo -e "build \$builddir/${exe}: link ${objects[@]}" >> "$TF"
  echo "  lflags = \$lflags_${name}" >> "$TF"
  echo "" >> "$TF"
done

sed -E "/CONFIG_REPLACE_BUILDS/r $TF" "build.in.ninja" \
| sed -E "/CONFIG_REPLACE_BUILDS/d" \
> build.ninja

rm "$TF"
