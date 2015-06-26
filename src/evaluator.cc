#include <fcntl.h>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "configrange.hh"
#include "evaluator.hh"
#include "network.cc"
#include "rat-templates.cc"

const unsigned int TICK_COUNT = 1000000;

Evaluator::Evaluator( const ConfigRange & range, JobGeneratorCommunicator& s_communicator )
  : _prng_seed( global_PRNG()() ), /* freeze the PRNG seed for the life of this Evaluator */
    _configs(),
    communicator( s_communicator )
{
  /* first load "anchors" */
  _configs.push_back( NetConfig().set_link_ppt( range.link_packets_per_ms.first ).set_delay( range.rtt_ms.first ).set_num_senders( range.max_senders ).set_on_duration( range.mean_on_duration ).set_off_duration( range.mean_off_duration ) );

  if ( range.lo_only ) {
    return;
  }

  _configs.push_back( NetConfig().set_link_ppt( range.link_packets_per_ms.first ).set_delay( range.rtt_ms.second ).set_num_senders( range.max_senders ).set_on_duration( range.mean_on_duration ).set_off_duration( range.mean_off_duration ) );
  _configs.push_back( NetConfig().set_link_ppt( range.link_packets_per_ms.second ).set_delay( range.rtt_ms.first ).set_num_senders( range.max_senders ).set_on_duration( range.mean_on_duration ).set_off_duration( range.mean_off_duration ) );
  _configs.push_back( NetConfig().set_link_ppt( range.link_packets_per_ms.second ).set_delay( range.rtt_ms.second ).set_num_senders( range.max_senders ).set_on_duration( range.mean_on_duration ).set_off_duration( range.mean_off_duration ) );

  /* now load some random ones just for fun */
  for ( int i = 0; i < 12; i++ ) {
    boost::random::uniform_real_distribution<> link_speed( range.link_packets_per_ms.first, range.link_packets_per_ms.second );
    boost::random::uniform_real_distribution<> rtt( range.rtt_ms.first, range.rtt_ms.second );
    boost::random::uniform_int_distribution<> num_senders( 1, range.max_senders );

    _configs.push_back( NetConfig().set_link_ppt( link_speed( global_PRNG() ) ).set_delay( rtt( global_PRNG() ) ).set_num_senders( num_senders( global_PRNG() ) ).set_on_duration( range.mean_on_duration ).set_off_duration( range.mean_off_duration ) );
  }
}

ProblemBuffers::Problem Evaluator::DNA( const WhiskerTree & whiskers,
    const NetConfig & config,
    const unsigned int ticks_to_run ) const
{
  ProblemBuffers::Problem ret;

  ret.mutable_whiskers()->CopyFrom( whiskers.DNA() );

  ProblemBuffers::ProblemSettings settings;
  settings.set_prng_seed( _prng_seed );
  settings.set_tick_count( ticks_to_run );

  ret.mutable_settings()->CopyFrom( settings );

  ret.mutable_config()->CopyFrom( config.DNA() );

  /*for ( auto &x : _configs ) {
    RemyBuffers::NetConfig *config = ret.add_configs();
    *config = x.DNA();
  }*/

  return ret;
}

AnswerBuffers::Result Evaluator::parse_problem_and_evaluate( const ProblemBuffers::Problem & problem )
{
  PRNG run_prng( problem.settings().prng_seed() );

  WhiskerTree run_whiskers = WhiskerTree( problem.whiskers() );
  run_whiskers.reset_counts();

  /* run tests */
  Network<Rat, Rat> network1( Rat( run_whiskers, false ), run_prng, problem.config() );
  network1.run_simulation( problem.settings().tick_count() );

  AnswerBuffers::Result result;
  
  result.set_score( network1.senders().utility() );

  for( auto & x : network1.senders().throughputs_delays() ){
    AnswerBuffers::SenderResults* res = result.add_results();
    res->set_throughput( x.first );
    res->set_delay( x.second );
  }

  return result;
}

/*AnswerBuffers::Outcome Evaluator::Outcome::DNA( void ) const
{
  AnswerBuffers::Outcome ret;

  for ( const auto & run : throughputs_delays ) {
    AnswerBuffers::ThroughputsDelays *tp_del = ret.add_throughputs_delays();
    tp_del->mutable_config()->CopyFrom( run.first.DNA() );

    for ( const auto & x : run.second ) {
      AnswerBuffers::SenderResults *results = tp_del->add_results();
      results->set_throughput( x.first ); 
      results->set_delay( x.second );
    }
  }

  ret.set_score( score );

  return ret;
}*/

/*Evaluator::Outcome::Outcome( const AnswerBuffers::Outcome & dna )
  : score( dna.score() ),
    throughputs_delays(),
    used_whiskers()
{
  for ( const auto &x : dna.throughputs_delays() ) {
    vector< pair< double, double > > tp_del;
    for ( const auto &result : x.results() ) {
      tp_del.emplace_back( result.throughput(), result.delay() );
    }

    throughputs_delays.emplace_back( NetConfig( x.config() ), tp_del );
  }
}*/

Evaluator::Outcome Evaluator::score( WhiskerTree & run_whiskers,
				     const bool trace, const unsigned int carefulness ) const
{
  return score( run_whiskers, _prng_seed, _configs, trace, TICK_COUNT * carefulness );
}


Evaluator::Outcome Evaluator::score( WhiskerTree & run_whiskers,
             const unsigned int prng_seed,
             const vector<NetConfig> & configs,
             const bool trace,
             const unsigned int ticks_to_run )
{
  PRNG run_prng( prng_seed );

  run_whiskers.reset_counts();

  /* run tests */
  Evaluator::Outcome the_outcome;
  for ( auto &x : configs ) {
    /* run once */
    Network<Rat, Rat> network1( Rat( run_whiskers, trace ), run_prng, x );
    network1.run_simulation( ticks_to_run );
    
    the_outcome.score += network1.senders().utility();
    the_outcome.throughputs_delays.emplace_back( x, network1.senders().throughputs_delays() );
  }

  the_outcome.used_whiskers = run_whiskers;

  return the_outcome;
}

Evaluator::Outcome Evaluator::distributed_score( WhiskerTree & run_whiskers, const unsigned int carefulness ) const {
  run_whiskers.reset_counts();

  /* run tests */
  Evaluator::Outcome the_outcome;

  vector< string > jobs;
  for ( auto & x : _configs ) {
    ProblemBuffers::Problem dna = DNA( run_whiskers, x, TICK_COUNT * carefulness );
    string job_str;
    dna.SerializeToString( &job_str );
    jobs.push_back( job_str );
  }

  auto results = communicator.assign_jobs( jobs );
    
  for ( auto & x : results ){
    AnswerBuffers::Result res;
    res.ParseFromString( x.get() );

    the_outcome.score += res.score();
    vector< pair< double, double > > results; // Format: (throughputs, delays)
    for ( auto & y : res.results() ) {
      results.emplace_back( make_pair( y.throughput(), y.delay() ) );
    }
    the_outcome.throughputs_delays.emplace_back( res.config(), results );
  }

  the_outcome.used_whiskers = run_whiskers;

  return the_outcome;
}
