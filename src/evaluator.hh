#ifndef EVALUATOR_HH
#define EVALUATOR_HH

#include <string>
#include <vector>

#include "answer.pb.h"
#include "communicator.hh"
#include "network.hh"
#include "problem.pb.h"
#include "random.hh"
#include "whiskertree.hh"

class Evaluator
{
public:
  class Outcome
  {
  public:
    double score;
    std::vector< std::pair< NetConfig, std::vector< std::pair< double, double > > > > throughputs_delays;
    WhiskerTree used_whiskers;

    Outcome() : score( 0 ), throughputs_delays(), used_whiskers() {}

    //Outcome( const AnswerBuffers::Outcome & dna );

    //AnswerBuffers::Outcome DNA( void ) const;
  };

private:
  const unsigned int _prng_seed;

  std::vector< NetConfig > _configs;

  JobGeneratorCommunicator& communicator;

public:
  Evaluator( const ConfigRange & range, JobGeneratorCommunicator& s_communicator );
  
  Outcome score( WhiskerTree & run_whiskers,
		 const bool trace = false,
		 const unsigned int carefulness = 1 ) const;

  static AnswerBuffers::Result parse_problem_and_evaluate( const ProblemBuffers::Problem & problem );

  Evaluator::Outcome distributed_score( WhiskerTree & run_whiskers, const unsigned int carefulness = 1 ) const;

  ProblemBuffers::Problem DNA( const WhiskerTree & whiskers, const NetConfig & config, const unsigned int ticks_to_run ) const;


  static Outcome score( WhiskerTree & run_whiskers,
			const unsigned int prng_seed,
			const std::vector<NetConfig> & configs,
			const bool trace,
			const unsigned int ticks_to_run );

};

#endif
