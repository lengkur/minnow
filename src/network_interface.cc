#include <iostream>

#include "../util/arp_message.hh"
#include "../util/ethernet_frame.hh"
#include "../util/exception.hh"
#include "../util/helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t ip = next_hop.ipv4_numeric();
  if ( arp_table_.contains( ip ) ) {
    EthernetFrame ether_frame;
    ether_frame.header.dst = arp_table_[ip].mac;
    ether_frame.header.src = ethernet_address_;
    ether_frame.header.type = EthernetHeader::TYPE_IPv4;
    ether_frame.payload = serialize( dgram );
    transmit( ether_frame );
    return;
  }
  if ( !arp_req_time_.contains( ip ) || current - arp_req_time_[ip] >= 5000 ) {
    EthernetFrame frame;
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = ethernet_address_;
    arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg.target_ethernet_address = {};
    arp_msg.target_ip_address = ip;

    frame.header.dst = ETHERNET_BROADCAST;
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.payload = serialize( arp_msg );
    transmit( frame );
    arp_req_time_[ip] = current;
  }
  waiting_dgrams_[ip].push( dgram );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    Parser p( frame.payload );
    InternetDatagram dgram;
    dgram.parse( p );
    // if ( dgram.header.dst == ip_address_.ipv4_numeric() ) {
    datagrams_received_.push( dgram );
    // } //以太网属于数据链路层，不用管ip
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    Parser p( frame.payload );
    ARPMessage arp_msg;
    arp_msg.parse( p );
    arp_table_[arp_msg.sender_ip_address] = { .mac = arp_msg.sender_ethernet_address, .time = current };
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      ARPMessage reply;
      reply.target_ip_address = arp_msg.sender_ip_address;
      reply.target_ethernet_address = arp_msg.sender_ethernet_address;
      reply.sender_ethernet_address = ethernet_address_;
      reply.sender_ip_address = ip_address_.ipv4_numeric();
      reply.opcode = ARPMessage::OPCODE_REPLY;

      EthernetFrame f;
      f.header.dst = arp_msg.sender_ethernet_address;
      f.header.src = ethernet_address_;
      f.header.type = EthernetHeader::TYPE_ARP;
      f.payload = serialize( reply );
      transmit( f );
    }
    if ( waiting_dgrams_.contains( arp_msg.sender_ip_address ) ) {
      auto& dgram_ipv4 = waiting_dgrams_[arp_msg.sender_ip_address];
      while ( !dgram_ipv4.empty() ) {
        InternetDatagram data = dgram_ipv4.front();
        dgram_ipv4.pop();
        EthernetFrame f;
        f.header.dst = arp_msg.sender_ethernet_address;
        f.header.src = ethernet_address_;
        f.payload = serialize( data );
        f.header.type = EthernetHeader::TYPE_IPv4;
        transmit( f );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current += ms_since_last_tick;
  for ( auto arp_msg = arp_table_.begin(); arp_msg != arp_table_.end(); ) {
    if ( current - arp_msg->second.time >= arp_cache_timeout_ ) {
      arp_msg = arp_table_.erase( arp_msg );
    } else {
      arp_msg++;
    }
  }
  for ( auto req = arp_req_time_.begin(); req != arp_req_time_.end(); ) {
    if ( current - req->second >= 5000 ) {
      waiting_dgrams_.erase( req->first );
      req = arp_req_time_.erase( req );
    } else {
      req++;
    }
  }
}
