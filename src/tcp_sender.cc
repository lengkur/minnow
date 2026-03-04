#include "tcp_sender.hh"
#include "../util/tcp_config.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_seqno_ - last_ack_;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t window_size = window_size_;
  if ( window_size == 0 ) {
    window_size = 1;
  }
  uint64_t available_space = window_size - sequence_numbers_in_flight();
  uint64_t payload_size {};
  TCPSenderMessage msg;
  while ( available_space > 0 ) {
    available_space = window_size - sequence_numbers_in_flight();
    if ( available_space == 0 ) {
      return;
    }
    msg = make_empty_message();
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    if ( !syn_sent_ ) {
      syn_sent_ = true;
      msg.SYN = true;
      available_space--;
    }
    payload_size = std::min( available_space, TCPConfig::MAX_PAYLOAD_SIZE );
    payload_size = std::min( payload_size, input_.reader().bytes_buffered() );
    msg.payload = input_.reader().peek().substr( 0, payload_size );
    input_.reader().pop( payload_size );
    if ( !fin_sent_ && input_.reader().is_finished() && available_space > payload_size ) {
      fin_sent_ = true;
      msg.FIN = true;
    }
    if ( msg.sequence_length() == 0 ) {
      break;
    }
    next_seqno_ += msg.sequence_length();
    transmit( msg );
    outsended_.push_back( msg );
    if ( !time_running_ ) {
      time_running_ = true;
      passwd_time_ = 0;
    }
    if ( msg.FIN ) {
      return;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  if ( input_.reader().has_error() ) {
    msg.RST = true;
  }
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.set_error();
    return;
  }
  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() ) {
    return;
  }
  uint64_t abs_ack = msg.ackno->unwrap( isn_, next_seqno_ );
  if ( abs_ack > next_seqno_ ) {
    return;
  }
  if ( abs_ack > last_ack_ ) {
    last_ack_ = abs_ack;
    current_RTO_ms_ = initial_RTO_ms_;
    consecutive_retransmissions_ = 0;
    passwd_time_ = 0;
  }
  while ( !outsended_.empty() ) {
    const auto& seg = outsended_.front();
    uint64_t seg_end = seg.seqno.unwrap( isn_, last_ack_ ) + seg.sequence_length();
    if ( seg_end <= abs_ack ) {
      outsended_.pop_front();
    } else {
      break;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( outsended_.empty() ) {
    return;
  }
  passwd_time_ += ms_since_last_tick;
  if ( passwd_time_ >= current_RTO_ms_ ) {
    transmit( outsended_.front() );
    if ( window_size_ > 0 ) {
      current_RTO_ms_ *= 2;
    }
    consecutive_retransmissions_++;
    passwd_time_ = 0;
  }
}
