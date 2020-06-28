#!/bin/bash -e
cd "$(dirname "$0")"

echo "Building hello1.s -> hello1.elf"
clang -nostdlib -O0 -o hello1.elf hello1.s

echo "Dumping disassemly hello1.elf -> hello1.elf.dis.txt"
llvm-objdump -D --syms --full-contents --all-headers hello1.elf > hello1.elf.dis.txt

echo "Running ./hello1.elf"
./hello1.elf
echo $?
