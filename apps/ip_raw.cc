#include "../util/socket.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

enum class ProtocolType : uint8_t
{
  RAW = 5,
  UDP = 17
};

class IPv4
{
public:
  uint8_t ihl_ = 5;
  uint8_t ver_ = 4;
  uint8_t tos_ {};
  uint16_t total_length_ {};
  uint16_t identification_ = 0xabcd;
  uint16_t fram_off_ {};
  uint8_t flag_ {};
  uint8_t ttl_ = 64;
  uint8_t protocol_ {};
  std::array<uint8_t, 4> source_ {};
  std::array<uint8_t, 4> destination_ {};
  uint16_t checksum_ = 0;
  std::string data_ {};

  explicit IPv4( const std::string& payload,
                 std::array<uint8_t, 4> source = { 127, 0, 0, 1 },
                 uint8_t Protocol = 5,
                 std::array<uint8_t, 4> destination = { 127, 0, 0, 1 } )
    : total_length_( 20 + payload.size() + ( static_cast<ProtocolType>( Protocol ) == ProtocolType::UDP ? 8 : 0 ) )
    , protocol_( Protocol )
    , source_( source )
    , destination_( destination )
    , data_( payload )
  {
    if ( data_.size() % 2 == 1 ) {
      data_.push_back( '\0' );
    }
    checksum_ = compute_checksum_();
  }

  std::string to_packet();
  std::uint16_t compute_checksum_();
};
class UDP : public IPv4
{
public:
  uint16_t source_port_ = 80;
  uint16_t destination_port_ = 80;
  uint16_t udp_length_ {};
  uint16_t udp_checksum_ {};

  explicit UDP( const std::string& payload,
                std::array<uint8_t, 4> source = { 127, 0, 0, 1 },
                uint16_t source_port = 80,
                std::array<uint8_t, 4> destination = { 127, 0, 0, 1 },
                uint16_t destination_port = 80 )
    : IPv4( payload, source, 17, destination )
    , source_port_( source_port )
    , destination_port_( destination_port )
    , udp_length_( static_cast<uint16_t>( 8 + payload.size() ) )
    , udp_checksum_( compute_udp_checksum_() )
  {}

  std::string to_udp_packet();
  std::uint16_t compute_udp_checksum_();
};
std::string IPv4::to_packet() // 把数据转成标准ip包流格式
{
  std::string packet( 20, '\0' );
  packet[0] = static_cast<char>( ( ver_ << 4 ) | ihl_ );
  packet[1] = static_cast<char>( tos_ );
  packet[2] = static_cast<char>( total_length_ >> 8 );
  packet[3] = static_cast<char>( total_length_ & 0x00ff );
  packet[4] = static_cast<char>( identification_ >> 8 );
  packet[5] = static_cast<char>( identification_ & 0x00ff );

  packet[6] = static_cast<char>( flag_ << 5 | ( fram_off_ >> 8 ) );
  packet[7] = static_cast<char>( fram_off_ & 0x00ff );

  packet[8] = static_cast<char>( ttl_ );
  packet[9] = static_cast<char>( protocol_ );

  packet[10] = static_cast<char>( checksum_ >> 8 );
  packet[11] = static_cast<char>( checksum_ & 0x00ff );

  packet[12] = static_cast<char>( source_[0] );
  packet[13] = static_cast<char>( source_[1] );
  packet[14] = static_cast<char>( source_[2] );
  packet[15] = static_cast<char>( source_[3] );

  packet[16] = static_cast<char>( destination_[0] );
  packet[17] = static_cast<char>( destination_[1] );
  packet[18] = static_cast<char>( destination_[2] );
  packet[19] = static_cast<char>( destination_[3] );
  return packet;
}
std::uint16_t IPv4::compute_checksum_() // 计算ip头checksum值
{
  std::string packet = to_packet();
  std::uint32_t sum = 0;
  packet[10] = packet[11] = 0;
  for ( std::size_t i = 0; i < 20; i += 2 ) {
    sum += ( static_cast<uint8_t>( packet.at( i ) ) << 0x8 ) | packet.at( i + 1 );
  }
  while ( sum >> 0x10 ) {
    sum = ( sum & 0xffff ) + ( sum >> 0x10 );
  }
  return checksum_ = static_cast<uint16_t>( ~sum );
}

std::string UDP::to_udp_packet() // 把数据转成标准udp包流格式
{
  std::string udp_packet = to_packet();
  udp_packet.resize( udp_packet.size() + 8 );
  udp_packet[20] = static_cast<char>( source_port_ >> 8 );
  udp_packet[21] = static_cast<char>( source_port_ & 0x00ff );
  udp_packet[22] = static_cast<char>( destination_port_ >> 8 );
  udp_packet[23] = static_cast<char>( destination_port_ & 0x00ff );
  udp_packet[24] = static_cast<char>( udp_length_ >> 8 );
  udp_packet[25] = static_cast<char>( udp_length_ & 0x00ff );
  udp_packet[26] = static_cast<char>( udp_checksum_ >> 8 );
  udp_packet[27] = static_cast<char>( udp_checksum_ & 0x00ff );

  return udp_packet + data_;
}

std::uint16_t UDP::compute_udp_checksum_() // 计算udp头的checksum值
{
  std::uint32_t sum = 0;
  std::uint32_t pseudo_sum = 0;
  pseudo_sum += static_cast<uint8_t>( source_[0] ) << 8 | source_[1];
  pseudo_sum += static_cast<uint8_t>( source_[2] ) << 8 | source_[3];
  pseudo_sum += static_cast<uint8_t>( destination_[0] ) << 8 | destination_[1];
  pseudo_sum += static_cast<uint8_t>( destination_[2] ) << 8 | destination_[3];
  pseudo_sum += ( 0 << 8 ) | protocol_;
  pseudo_sum += udp_length_;

  sum += pseudo_sum;
  sum += source_port_;
  sum += destination_port_;
  sum += udp_length_;
  for ( size_t i = 0; i < data_.size(); i += 2 ) {
    sum += ( static_cast<uint8_t>( data_.at( i ) ) << 0x8 ) | data_.at( i + 1 );
  }
  while ( sum >> 0x10 ) {
    sum = ( sum & 0xffff ) + ( sum >> 0x10 );
  }
  return udp_checksum_ = static_cast<uint16_t>( ~sum );
}

namespace {
std::array<uint8_t, 4> ip_to_array( const std::string& target ) // 把ipv4地址四部分转成数组
{
  std::array<uint8_t, 4> result {};
  uint8_t sum = 0;
  size_t n = 0;
  for ( auto i : target ) {
    if ( '0' <= i && i <= '9' ) {
      sum = ( sum * 10 ) + i - '0';
    } else if ( i == '.' ) {
      result.at( n++ ) = sum;
      sum = 0;
    }
  }
  result.at( n ) = sum;
  return result;
}
} // namespace

int main()
{
  // construct an Internet or user datagram here, and send using a RawSocket
  RawSocket server;
  std::string data = "Hello,I'm Lengkur.'";
  std::string source_ip = server.local_address().ip(); // 获取本机地址
  std::string destination_ip = "127.0.0.1";
  IPv4 payload(
    data, ip_to_array( source_ip ), static_cast<uint8_t>( ProtocolType::RAW ), ip_to_array( destination_ip ) );

  server.send( payload.to_packet() + payload.data_, Address( destination_ip ) ); // 测试protocol5的ip包

  UDP payload_udp( data, ip_to_array( source_ip ), 80, ip_to_array( destination_ip ), 80 );
  server.send( payload_udp.to_udp_packet(), Address( destination_ip ) ); // 测试protocol17的ip包
  return 0;
}
