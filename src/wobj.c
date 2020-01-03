// mini object library
#define W_REF_DEBUG
#define W_ALLOC_DEBUG
#define W_DEALLOC_DEBUG


#define WObj_HEAD WObj obase;
#define WObj_HEAD_INIT(type) { 1, type }
#define WObj_HEAD_INIT_PERMANENT(type) { 2, type }  /* never dealloc */

struct _wtype;

// data object
typedef struct _wobj {
  u32               refcount;
  struct _WTypeObj* type;
} WObj;

typedef void (*WDestructor)(WObj*);

// type object
typedef struct _WTypeObj {
  WObj_HEAD
  const char*  name;
  size_t       size;  // size of data object (including size of WObj)
  WDestructor  destructor;
} WTypeObj;

// Root type
static WTypeObj WTypeType = { WObj_HEAD_INIT_PERMANENT(NULL), "Type", sizeof(WObj), NULL };

inline static const WTypeObj* WType(const void* o) { return ((WObj*)o)->type; }

inline static u32  WRefCount(const void* o) { return ((WObj*)o)->refcount; }
inline static void WIncRef(void* o) { ((WObj*)o)->refcount++; }

static void WDealloc(WObj*);

// WDecRef
#ifdef W_REF_DEBUG
static void WReportNegativeRefcount(WObj* o, const char* file, int line) {
  fprintf(stderr, "WObj@%p negative refcount at %s:%d\n", o, file, line);
}
static inline void _WDecRef(WObj* o, const char* file, int line) {
  if (--o->refcount != 0) {
    if (o->refcount == (u32)(0xFFFFFFFF)) {
      WReportNegativeRefcount(o, file, line);
    }
  // quoet -Wunused-parameter
  (void)file;
  (void)line;
  } else {
    WDealloc(o);
  }
}
#define WDecRef(o) _WDecRef((WObj*)(o), __FILE__, __LINE__)
#else
static inline void _WDecRef(WObj* o) {
  if (--o->refcount == 0) {
    WDealloc(o);
  }
}
#define WDecRef(o) _WDecRef((WObj*)(o))
#endif

inline static void WXIncRef(void* o) { if (o != NULL) { WIncRef(o); } }
inline static void WXDecRef(void* o) { if (o != NULL) { WDecRef(o); } }


// o = NULL
#define WClear(o)           \
  do {                      \
    WObj* tmp = (WObj*)(o); \
    if (tmp != NULL) {      \
      (o) = NULL;           \
      WDecRef(tmp);         \
    }                       \
  } while (0)


// dst = src
#define WSetRef(dst, src)     \
  do {                        \
    WObj* tmp = (WObj*)(dst); \
    (dst) = (src);            \
    WDecref(tmp);             \
  } while (0)


static void WDealloc(WObj* o) {
  #ifdef W_DEALLOC_DEBUG
  printf("dealloc %s@%p\n", o->type ? o->type->name : "(unknown_type)", o);
  #endif
  if (o->type) {
    if (o->type->destructor) {
      o->type->destructor(o);
    }
    WDecRef(o->type);
  }
  free(o);
}

static WObj* _WObjNewVar(WTypeObj* type, size_t extrasize) {
  auto o = (WObj*)malloc(type->size + extrasize);
  o->refcount = 1;
  o->type = type;
  WXIncRef(type);
  #ifdef W_ALLOC_DEBUG
  printf("alloc %s@%p\n", type ? type->name : "(unknown_type)", o);
  #endif
  return o;
}

inline static WObj* _WObjNew(WTypeObj* type) {
  return _WObjNewVar(type, 0);
}

#define WObjNew(type, typeobj)               ((type*)_WObjNew(typeobj))
#define WObjNewVar(type, typeobj, extrasize) ((type*)_WObjNewVar(typeobj, (extrasize)))
