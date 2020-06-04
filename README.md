# wlang

Programming language in development

```txt
./dev.sh example/factorial.w
```

Notes:

- Implemented in C11.
- Uses dlmalloc instead of libc malloc, making it portable to wasm etc.
- `./dev.sh -analyze`        — run incremental code analyzer
- `./dev.sh -lldb <srcfile>` — build and run product in debugger, watching source files for changes
- `./dev.sh <srcfile>`       — build and run product, watching source files for changes
- `./build.sh`               — build release product and exit
- `./build.sh -g`            — build debug product and exit
- `./build.sh -analyze`      — analyze entire project using ([Infer](https://fbinfer.com/))
- Debug products are built with Clang address sanitizer by default.
  To disable asan/msan, edit the `build.in.ninja` file.

Requirements for building:

- [clang](https://clang.llvm.org/) version >=10
- [Ninja](https://ninja-build.org/) version >=1.2
- Bash or a bash-compatible shell, for running the build scripts
- [Python 3](https://www.python.org/) used for code generation
- [Infer](https://fbinfer.com/) used for code analysis (optional)

If you're on macOS, install everything you need with `brew install clang python ninja infer`.
