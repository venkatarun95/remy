#include <cassert>
#include <utility>

#include "rat.hh"

using namespace std;

template <class NextHop>
void Rat::send( const unsigned int id, NextHop & next, const double & tickno,
		const int packets_sent_cap )
{
  assert( _packets_sent >= _largest_ack + 1 );

  if ( _the_window == 0 ) {
    /* initial window and intersend time */
    const Whisker & current_whisker( _whiskers.use_whisker( _memory, _track ) );
    _the_window = current_whisker.window( _the_window );
    _intersend_time = current_whisker.intersend();
    //assert(_the_window != 0 ); //edit - venkat - just to ensure that a sender doesn't stay 0 forever because right now, I believe that memory will never be called if no packets are sent. But something tells me that my understanding is incorrect
  }

  if ( (_packets_sent < _largest_ack + 1 + _the_window)
       and (_last_send_time + _intersend_time <= tickno) ) {

    /* Have we reached the end of the flow for now? */
    if ( _packets_sent >= packets_sent_cap ) {
      return;
    }

    Packet p( id, _flow_id, tickno, _packets_sent );
    _packets_sent++;
    _memory.packet_sent( p );
    next.accept( p, tickno );
    _last_send_time = tickno;
  }
}