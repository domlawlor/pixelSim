#pragma once

#define global static

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#include <stdint.h> // Safe include containing only type definitions
#define s8  int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define r32 float
#define r64 double

#define b32 s32
#define memIdx size_t

#define  U8_MAX UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

#define S8_MIN INT8_MIN
#define S8_MAX INT8_MAX
#define S16_MIN INT16_MIN
#define S16_MAX INT16_MAX
#define S32_MIN INT32_MIN
#define S32_MAX INT32_MAX
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX

#include <cfloat>
#define R32_MAX FLT_MAX
#define R64_MAX DBL_MAX

#define Assert(Expression) do { if(!(Expression)) {*(int *)0 = 0;} } while(false)
#define AssertMsg(Expression, MSG) Assert(Expression)

#define AssertIndex(index, max) Assert(index >= 0 && index < max)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

#define OffsetOf(type, Member) &(((type *)0)->Member)

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)

#define UNUSED(x) ((void)(x))