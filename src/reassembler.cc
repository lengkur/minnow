#include "reassembler.hh"
#include "byte_stream.hh"

std::string Reassembler::proper_data( uint64_t& first_index, std::string data ) // 修剪data
{
  auto it = buffer_.lower_bound( first_index );
  if ( it != buffer_.begin() ) { // 处理前面覆盖部分
    it = std::prev( it );
    if ( it->first + it->second.size() > first_index ) {
      uint64_t overlap = it->first + it->second.size() - first_index;
      if ( overlap >= data.size() ) {
        return "";
      }
      data = data.substr( overlap );
      first_index += overlap;
    }
  }
  it = buffer_.lower_bound( first_index );
  uint64_t new_last = first_index + data.size();
  while ( !buffer_.empty() && it != buffer_.end() ) { // 处理后面覆盖部分
    if ( it->first >= new_last ) {
      break;
    }
    uint64_t it_last_index = it->first + it->second.size();
    if ( new_last >= it_last_index ) {
      bytes_pending -= it->second.size();
      buffer_.erase( it );
      it = buffer_.lower_bound( first_index );
      continue;
    }
    if ( new_last > it->first && it_last_index > new_last ) {
      data = data.substr( 0, it->first - first_index );
      break;
    }
  }
  return data;
}

void Reassembler::insert( uint64_t first_index, std::string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    is_end = true;
    data_end_index_ = first_index + data.size();
  }
  uint64_t win_start = next_index_;
  uint64_t win_end = next_index_ + writer().available_capacity();
  uint64_t last_index = first_index + data.size();
  if ( last_index < win_start || first_index >= win_end ) {
    return;
  }
  if ( last_index > win_end ) { // 51~63行代码负责确保新TCP段在TCP窗口内，方便后续操作
    data = data.substr( 0, win_end - first_index );
  }
  if ( next_index_ > first_index ) {
    data = data.substr( next_index_ - first_index );
    first_index = next_index_;
  }
  if ( data.empty() ) { // 检查改后的data是否为空，如果正好到最后直接关闭输入流并返回
    if ( is_end && next_index_ == data_end_index_ ) {
      output_.writer().close();
    }
    return;
  }
  data = proper_data( first_index, data );
  if ( data.empty() ) { // 如果改后data为空直接返回
    return;
  }
  buffer_[first_index] = data; // 将修剪后的data写入缓冲区
  bytes_pending += data.size();
  while ( !buffer_.empty() ) { // 从最前面索引开始，如果已经组成连续段直接写入字节流
    auto it = buffer_.begin();
    if ( it->first == next_index_ ) {
      const std::string& result = it->second;
      output_.writer().push( result );
      next_index_ += result.size();
      bytes_pending -= result.size();
      buffer_.erase( it );
    } else {
      break;
    }
  }
  if ( is_end && next_index_ == data_end_index_ ) { // 如果到结束段关闭输入流
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return bytes_pending;
}
