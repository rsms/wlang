#pragma once

typedef struct StrMap { void* root; } StrMap;
#define StrMap_INIT { NULL }

void   StrMapInit(StrMap*);
void   StrMapClear(StrMap*);
size_t StrMapLen(const StrMap*);  // number of entries in m
bool   StrMapEmpty(const StrMap*); // true if empty
Str    StrMapRepr(const StrMap*, Str s);
void*  StrMapGet(const StrMap*, Str k);
void   StrMapSet(StrMap*, Str k, void* v);  // add or replace
bool   StrMapAdd(StrMap*, Str k, void* v);  // true if added
void   StrMapDelete(StrMap*, Str k);
