#ifndef RATBREEDER_HH
#define RATBREEDER_HH

#include <unordered_map>
#include <boost/functional/hash.hpp>

#include "communicator.hh"
#include "configrange.hh"
#include "evaluator.hh"

class WhiskerImprover
{
private:
  const Evaluator eval_;

  WhiskerTree rat_;

  std::unordered_map< Whisker, double, boost::hash< Whisker > > eval_cache_ {};

  double score_to_beat_;

public:
  WhiskerImprover( const Evaluator & evaluator, const WhiskerTree & rat, const double score_to_beat );
  double improve( Whisker & whisker_to_improve );
};

class RatBreeder
{
private:
  ConfigRange _range;
  JobGeneratorCommunicator& communicator;

  void save_temp_whiskers( WhiskerTree & whiskers ) const;
  void apply_best_split( WhiskerTree & whiskers, const unsigned int generation ) const;

public:
  RatBreeder( const ConfigRange & s_range, JobGeneratorCommunicator& s_communicator ) : _range( s_range ), communicator( s_communicator ) {}

  Evaluator::Outcome improve( WhiskerTree & whiskers );
};

#endif
