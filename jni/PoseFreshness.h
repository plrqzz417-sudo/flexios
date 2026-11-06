#pragma once
inline constexpr bool PoseFresh(unsigned room,unsigned sample_room,long long age_us,unsigned mask) {
    return room==sample_room&&age_us>=0&&age_us<=100000&&mask!=0;
}
