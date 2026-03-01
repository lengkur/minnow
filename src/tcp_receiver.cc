#include "tcp_receiver.hh"
#include "wrapping_integers.hh"

using namespace std;

void TCPReceiver::receive( const TCPSenderMessage& message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( message.SYN ) {
    isn_ = message.seqno;
  }
  if ( isn_.has_value() ) {
    uint64_t first_index = message.seqno.unwrap( isn_.value(), reassembler_.writer().bytes_pushed() );
    first_index = message.SYN ? 0 : first_index - 1;
    reassembler_.insert( first_index, message.payload, message.FIN );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  if ( isn_.has_value() ) {
    const uint64_t absolutial_ack = 1 + reassembler_.writer().bytes_pushed() + writer().is_closed();
    message.ackno = Wrap32::wrap( absolutial_ack, isn_.value() );
  }
  message.window_size = std::min( reassembler_.writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) );
  message.RST = reassembler_.reader().has_error();
  return message;
}
