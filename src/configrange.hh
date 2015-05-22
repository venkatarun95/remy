#ifndef CONFIG_RANGE_HH
#define CONFIG_RANGE_HH

#include "dna.pb.h"

class ConfigRange
{
public:
  // throughput in mb/s
  std::pair< double, double > link_packets_per_ms { 1, 2 };
  // Queue size in link
  //   Default value is calculated as 0.1 * link_ppt * rtt_ms for min and max values.
  //   Bandwidth-delay product is used rather than a lower value as the number of senders is small 
  //   and hence we can assume synchronization of the sawtooths.
  //   Refer "Recent Results on Sizing Router Buffers" by Appenzler et al. for more details
  std::pair< unsigned int, unsigned int > link_limit { 10, 40 };
  // round trip time in link. $$Not sure what this does
  std::pair< double, double > rtt_ms { 100, 200 };
  unsigned int min_senders { 1 };
  unsigned int max_senders { 16 };
  double mean_on_duration { 1000 };
  double mean_off_duration { 1000 };
  // If true, Evaluator loads all corner points in the hypercube as configurations
  // else it loads only the lower limit and chooses all other configurations randomly
  bool lo_only { false };

  RemyBuffers::ConfigRange DNA( void ) const;
};

#endif  // CONFIG_RANGE_HH
