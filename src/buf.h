#pragma once

typedef struct Buf {
  Memory mem;
  u8*    ptr;
  size_t cap;
  size_t len;
} Buf;

void BufInit(Buf* nonull b, Memory nullable mem, size_t cap);
void BufFree(Buf* nonull b);
static void BufMakeRoomFor(Buf* nonull b, size_t size); // ensures free space for at least size
void BufAppend(Buf* nonull b, const void* nonull ptr, size_t size);
void BufAppendFill(Buf* nonull b, u8 v, size_t size); // append size bytes of value v
static void BufAppendc(Buf* b, char c); // append one byte
u8*  BufAlloc(Buf* nonull b, size_t size); // like BufAppend but leaves allocated data untouched.
u8*  BufAllocz(Buf* nonull b, size_t size); // zeroes segment

void _BufMakeRoomFor(Buf* b, size_t size);

inline static void BufAppendc(Buf* b, char c) {
  if (b->cap <= b->len) { _BufMakeRoomFor(b, 1); }
  b->ptr[b->len++] = (u8)c;
}

inline static void BufMakeRoomFor(Buf* b, size_t size) {
  if (b->cap - b->len < size) {
    _BufMakeRoomFor(b, size);
  }
}


// DefArrayBuffer allows defining a type and a set of porcelain functions around Buf
// to make it act as an array holding elements of any type (ElemT).
//
// Prototypes:
//
//  void   ArrayT##Init(ArrayT* nonull a, Memory nullable mem, size_t cap);
//  void   ArrayT##Free(ArrayT* nonull a);
//  ElemT* ArrayT##At(ArrayT* nonull a, size_t index);
//  void   ArrayT##Push(ArrayT* nonull a, ElemT v);
//  ElemT  ArrayT##Pop(ArrayT* nonull a);
//  ElemT* ArrayT##Alloc(ArrayT* nonull a, size_t count);
//  void   ArrayT##MakeRoomFor(ArrayT* nonull a, size_t count);
//
#define DefArrayBuffer(ArrayT, ElemT) \
  typedef Buf ArrayT; \
  inline static void ArrayT##Init(ArrayT* nonull a, Memory nullable mem, size_t cap) { \
    BufInit(a, mem, cap * sizeof(ElemT)); \
  } \
  inline static void ArrayT##Free(ArrayT* nonull a) { BufFree(a); } \
  inline static ElemT* ArrayT##At(ArrayT* nonull a, size_t i) { \
  	return (ElemT*)&a->ptr[i * sizeof(ElemT)]; \
  } \
  inline static void ArrayT##Push(ArrayT* nonull a, ElemT v) { BufAppend(a, &v, sizeof(ElemT)); }\
  inline static ElemT ArrayT##Pop(ArrayT* nonull a) { \
  	a->len -= sizeof(ElemT); \
  	return *(ElemT*)&a->ptr[a->len]; \
  } \
  inline static ElemT* ArrayT##Alloc(ArrayT* nonull a, size_t count) { \
    return (ElemT*)BufAlloc(a, count * sizeof(ElemT)); \
  } \
  inline static void ArrayT##MakeRoomFor(ArrayT* nonull a, size_t count) { \
    BufMakeRoomFor(a, count * sizeof(ElemT)); \
  } \
/* DefArrayBuffer */


#define ArrayBufferForEach(b, ELEMTYPE, LOCALNAME)      \
  /* this "for" introduces LOCALNAME */                 \
  for (auto LOCALNAME = (ELEMTYPE*)&((b)->ptr[0]),      \
  	        LOCALNAME##__guard = (ELEMTYPE*)NULL;       \
       LOCALNAME##__guard == NULL;                      \
       LOCALNAME##__guard++)                            \
  /* actual for loop */                                 \
  for (                                                 \
    u32 LOCALNAME##__i = 0,                             \
        LOCALNAME##__end = (b)->len;                    \
    LOCALNAME = (ELEMTYPE*)&((b)->ptr[LOCALNAME##__i]), \
    LOCALNAME##__i < LOCALNAME##__end;                  \
    LOCALNAME##__i += sizeof(ELEMTYPE)                  \
  ) /* <body should follow here> */

