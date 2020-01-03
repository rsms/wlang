#include "wp.h"

u8* readfile(const char* filename, size_t* bufsize_out) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    return NULL;
  }

  size_t bufsize = (size_t)st.st_size;
  u8* buf = (u8*)malloc(bufsize); // TODO use feelist

  auto nread = read(fd, buf, bufsize);
  close(fd);
  if (nread != bufsize) {
    die("TODO: parsefile read() partial");
  }
  if (nread < 0) {
    free(buf);
    return NULL;
  }

  *bufsize_out = bufsize;
  return buf;
}

