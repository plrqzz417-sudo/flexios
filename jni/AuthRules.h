#pragma once
#include <cstdint>
namespace auth_rules {
inline constexpr bool fresh(int64_t now,int64_t stamp) {return stamp>=now-25&&stamp<=now+25;}
inline constexpr bool active(bool verified,int64_t now,int64_t deadline) {return verified&&now<deadline;}
}
