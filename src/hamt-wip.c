static const u32 HAMT_BITS     = 5;                 // 5, 4, 3, 2 ...
static const u32 HAMT_BRANCHES = 1 << HAMT_BITS;    // 2^5=32, 2^4=16, 2^3=8, 2^2=4 ...
static const u32 HAMT_MASK     = HAMT_BRANCHES - 1; // 31 (or 0x1f), 15, 7, 3 ...


typedef struct {
  WObj_HEAD
  // PyHamtNode* root;
  // PyObject*   weakreflist;
  ssize_t count;
} HamtObj;

static void HamtDealloc(WObj* o) {
  HamtObj* h = (HamtObj*)o;
  dlog("HamtDealloc");
}

static WTypeObj HamtType = {
  WObj_HEAD_INIT_PERMANENT(&WTypeType),
  "Hamt",
  sizeof(HamtObj),
  (WDestructor)&HamtDealloc,
};


static HamtObj* HamtNew() {
  auto h = WObjNew(HamtObj, &HamtType);
  h->count = 0;
  return h;
}

__attribute__((constructor)) static void test() {
  dlog("TEST HAMT");

  auto h = HamtNew();
  h->count++;
  dlog("h->count = %zd", h->count);
  WDecRef(h);

}

// /* Create a new HAMT immutable mapping. */
// PyHamtObject * _PyHamt_New(void);

// /* Return a new collection based on "o", but with an additional
//    key/val pair. */
// PyHamtObject * _PyHamt_Assoc(PyHamtObject *o, PyObject *key, PyObject *val);

// /* Return a new collection based on "o", but without "key". */
// PyHamtObject * _PyHamt_Without(PyHamtObject *o, PyObject *key);

// Find "key" in the "o" collection.
// Return:
// - -1: An error occurred.
// - 0: "key" wasn't found in "o".
// - 1: "key" is in "o"; "*val" is set to its value (a borrowed ref).
int HamtFind(HamtObj *o, WObj *key, WObj **val);

