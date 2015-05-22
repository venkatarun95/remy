#ifndef PACKET_HH
#define PACKET_HH

class Packet
{
public:
  unsigned int src;
  unsigned int flow_id;
  // tick_sent is filled by Rat::packets_received which in turn is called by SwitchedSender::?::tick. 
  // tick_received is filled by
  double tick_sent, tick_received;
  int seq_num;

  Packet( const unsigned int & s_src,
	  const unsigned int & s_flow_id,
	  const double & s_tick_sent,
	  const unsigned int & s_seq_num )
    : src( s_src ),
      flow_id( s_flow_id ), tick_sent( s_tick_sent ),
      tick_received( -1 ),
      seq_num( s_seq_num )
  {}
};

#endif
