#include <unistd.h> // sysconf
#include <sys/errno.h>

#include "defs.h"
#include "os.h"

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


u8* os_readfile(const char* filename, size_t* size_inout, Memory mem) {
  assert(size_inout != NULL);

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return NULL;
  }

  size_t bufsize = (size_t)st.st_size;
  size_t limit = *size_inout;
  if (limit > 0 && limit < bufsize) {
    bufsize = limit;
  }

  u8* buf = (u8*)memalloc(mem, bufsize);

  auto nread = read(fd, buf, bufsize);
  close(fd);
  if (nread < 0) {
    memfree(mem, buf);
    *size_inout = 0;
    return NULL;
  }

  assert(nread == bufsize);

  *size_inout = bufsize;
  return buf;
}


bool os_writefile(const char* filename, const void* ptr, size_t size) {
  FILE* fp = fopen(filename, "w");
  if (fp == NULL) {
    return false;
  }
  auto z = fwrite(ptr, size, 1, fp);
  fclose(fp);
  return size == 0 ? z == 0 : z == 1;
}

