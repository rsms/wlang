#include "wp.h"

#include <unistd.h> // sysconf


static size_t _mempagesize = 0;

size_t os_mempagesize() {
  if (_mempagesize == 0) {
    auto z = sysconf(_SC_PAGESIZE);
    if (z <= 0) {
      _mempagesize = 1024 * 4; // usually 4kB
    } else {
      _mempagesize = (size_t)z;
    }
  }
  return _mempagesize;
}


u8* os_readfile(const char* filename, size_t* bufsize_out, Memory mem) {
  // TODO: use mmap

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    return NULL;
  }

  size_t bufsize = (size_t)st.st_size;
  u8* buf = (u8*)memalloc(mem, bufsize); // TODO use feelist

  auto nread = read(fd, buf, bufsize);
  close(fd);
  if (nread != bufsize) {
    die("TODO: parsefile read() partial");
  }
  if (nread < 0) {
    memfree(mem, buf);
    return NULL;
  }

  *bufsize_out = bufsize;
  return buf;
}

