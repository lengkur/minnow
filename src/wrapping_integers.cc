#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { zero_point + n };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t offset = raw_value_ - zero_point.raw_value_;              // 算出偏移量
  uint64_t candidate = ( checkpoint & 0xffffffff00000000 ) | offset; // 只保留checkpoint前32位，然后加上偏移量
  if ( candidate > checkpoint + ( 1UL << 31 )
       && ( candidate >= ( 1UL << 32 ) ) ) { // candidate比checkpoint大太多，就减一个周期，确保减后不为复数
    candidate -= 1UL << 32;
  } else if ( candidate + ( 1UL << 31 ) < checkpoint ) { // 比checkpoint小太多就加一个周期
    candidate += 1UL << 32;
  }
  return candidate; // 将离checkpoint最近的索引返回
}
