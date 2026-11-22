#pragma once
#include <stdint.h>
namespace wire {
using u32 = uint32_t;
using f32 = float;
#pragma pack(push, 1)
struct Frame {
#define FIELD(type, name, count) type name[count];
#include "fields.def"
#undef FIELD
};
#pragma pack(pop)
static_assert(sizeof(float) == 4, "需要IEEE754 float32");
} // namespace wire
