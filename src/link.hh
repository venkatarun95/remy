#ifndef LINK_HH
#define LINK_HH

#include <queue>

#include <boost/random/uniform_01.hpp>

#include "packet.hh"
#include "delay.hh"
#include "random.hh"

// Simulates a link.
//  Accepts packets and forward them to NextHop (supplied by tick function in network.cc)
class Link
{
private:
  std::queue< Packet > _buffer;

  Delay _pending_packet;

  unsigned int _limit;

  double _drop_rate;

  PRNG & _prng;

  boost::random::uniform_01<> distribution;

public:
  Link( const double s_rate,
	const unsigned int s_limit,
  const double s_drop_rate,
	PRNG & s_prng )
    : _buffer(), _pending_packet( 1.0 / s_rate ), _limit( s_limit ), _drop_rate(s_drop_rate), _prng( s_prng ), distribution() {}

  void accept( const Packet & p, const double & tickno ) noexcept {
    if ( _pending_packet.empty() ) {
      _pending_packet.accept( p, tickno );
    } else {
      if ( _buffer.size() < _limit ) { // congestive loss
        if ( distribution( _prng )  > _drop_rate ){ // stochastic loss
          _buffer.push( p );
        }
      }
      // else, drop the packet
    }
  }

  template <class NextHop>
  void tick( NextHop & next, const double & tickno );

  double next_event_time( const double & tickno ) const { return _pending_packet.next_event_time( tickno ); }
};

#endif
