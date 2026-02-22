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
  uint16_t identification_ {};
  uint16_t fram_off_ {};
  uint8_t flag_ {};
  uint8_t ttl_ = 64;
  uint8_t protocol_ {};
  uint16_t checksum_ {};
  std::array<uint8_t, 4> source_;
  std::array<uint8_t, 4> destination_;

  std::string to_packet();
  explicit IPv4( std::size_t PayloadLength,
                 std::array<uint8_t, 4> source = { 127, 0, 0, 1 },
                 uint8_t Protocol = 5,
                 std::array<uint8_t, 4> destination = { 127, 0, 0, 1 } )
    : total_length_( 20 + PayloadLength ), protocol_( Protocol ), source_( source ), destination_( destination )
  {
    std::string packet = to_packet();
    std::uint32_t sum = 0;
    for ( std::size_t i = 0; i < 20; i += 2 ) {
      sum += ( packet.at( i ) << 0x8 ) | packet.at( i + 1 );
    }
    while ( sum >> 0x10 ) {
      sum = ( sum & 0xffff ) + ( sum >> 0x10 );
    }
    checksum_ = static_cast<uint16_t>( ~sum );
  }
};

std::string IPv4::to_packet()
{
  std::string packet( 20, '\0' );
  packet[0] = static_cast<char>( ( ver_ << 4 ) | ihl_ );
  packet[1] = static_cast<char>( tos_ );
  packet[2] = static_cast<char>( total_length_ >> 8 );
  packet[3] = static_cast<char>( total_length_ & 0x00ff );
  packet[4] = static_cast<char>( identification_ >> 8 );
  packet[5] = static_cast<char>( identification_ & 0x00ff );

  packet[6] = static_cast<char>( flag_ << 5 | ( this->fram_off_ >> 8 ) );
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

namespace {
std::array<uint8_t, 4> ip_to_array( const std::string& target )
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
  std::string source_ip = server.local_address().ip();
  std::string destination_ip = "127.0.0.1";
  IPv4 payload( data.size(),
                ip_to_array( source_ip ),
                static_cast<uint8_t>( ProtocolType::RAW ),
                ip_to_array( destination_ip ) );

  server.send( payload.to_packet() + data, Address( destination_ip ) );
  return 0;
}
