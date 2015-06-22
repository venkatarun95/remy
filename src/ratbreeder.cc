#include <iostream>
#include <vector>
#include <cassert>
#include <future>
#include <thread>
#include <limits>

#include "ratbreeder.hh"

using namespace std;

void RatBreeder::apply_best_split( WhiskerTree & whiskers, const unsigned int generation ) const
{
  const Evaluator eval( _range );
  auto outcome( eval.score( whiskers, true ) );

  while ( 1 ) {
    auto my_whisker( outcome.used_whiskers.most_used( generation ) );
    assert( my_whisker );

    WhiskerTree bisected_whisker( *my_whisker, true );

    if ( bisected_whisker.num_children() == 1 ) {
      //      printf( "Got unbisectable whisker! %s\n", my_whisker->str().c_str() );
      auto mutable_whisker( *my_whisker );
      mutable_whisker.promote( generation + 1 );
      assert( outcome.used_whiskers.replace( mutable_whisker ) );
      continue;
    }

    //    printf( "Bisecting === %s === into === %s ===\n", my_whisker->str().c_str(), bisected_whisker.str().c_str() );
    assert( whiskers.replace( *my_whisker, bisected_whisker ) );
    break;
  }
}

Evaluator::Outcome RatBreeder::improve( WhiskerTree & whiskers )
{
  /* back up the original whiskertree */
  /* this is to ensure we don't regress */
  WhiskerTree input_whiskertree( whiskers );

  /* evaluate the whiskers we have */
  whiskers.reset_generation();
  unsigned int generation = 0;

  while ( generation < 5 ) {
    const Evaluator eval( _range );

    auto outcome( eval.score( whiskers ) );

    /* is there a whisker at this generation that we can improve? */
    auto most_used_whisker_ptr = outcome.used_whiskers.most_used( generation );

    /* if not, increase generation and promote all whiskers */
    if ( !most_used_whisker_ptr ) {
      generation++;
      whiskers.promote( generation );
      continue;
    }

    WhiskerImprover improver( eval, whiskers, outcome.score );

    Whisker whisker_to_improve = *most_used_whisker_ptr;

    double score_to_beat = outcome.score;

    //while ( 1 ) {
      double new_score = improver.improve( whisker_to_improve );
      assert( new_score >= score_to_beat );

      //cerr << "Score jumps from " << score_to_beat << " to " << new_score << endl;
      score_to_beat = new_score;
      /*if ( new_score == score_to_beat ) {
        cerr << "Ending search." << endl;
        break;
      } else {
        cerr << "Score jumps from " << score_to_beat << " to " << new_score << endl;
        score_to_beat = new_score;
      }*/
    //}

    whisker_to_improve.demote( generation + 1 );

    const auto result __attribute((unused)) = whiskers.replace( whisker_to_improve );
    assert( result );
  }

  /* Split most used whisker */
  apply_best_split( whiskers, generation );

  /* carefully evaluate what we have vs. the previous best */
  const Evaluator eval2( _range );
  const auto new_score = eval2.score( whiskers, false, 10 );
  const auto old_score = eval2.score( input_whiskertree, false, 10 );

  if ( old_score.score >= new_score.score ) {
    fprintf( stderr, "Regression, old=%f, new=%f\n", old_score.score, new_score.score );
    whiskers = input_whiskertree;
    return old_score;
  }

  return new_score;
}

WhiskerImprover::WhiskerImprover( const Evaluator & s_evaluator,
				  const WhiskerTree & rat,
				  const double score_to_beat )
  : eval_( s_evaluator ),
    rat_( rat ),
    score_to_beat_( score_to_beat )
{}

double WhiskerImprover::improve( Whisker & whisker_to_improve )
{
  static int direction_id = 0 % 6; //direction in which to start exploring
  int search_mode = 0; // 0 - explore several directions; > 0 - explore one direction, value indicates number of actions already explored in this direction
  int num_evaluated = 0; // for use when search_mode = 0

  int num_threads = thread::hardware_concurrency ();
  assert( num_threads != 0 );

  vector< pair< const Whisker &, future< pair< bool, double > > > > scores;

  double prev_score = score_to_beat_;
  cout << "Prev score: " << prev_score << endl;
  while ( true ) { 
    int prev_direction_id = direction_id;
    cout << " Direction: " << direction_id << " Mode: " << search_mode << endl;
    auto replacements( whisker_to_improve.next_generation( direction_id, search_mode, num_threads ) );
    if( search_mode == 0)
      num_evaluated += (direction_id > prev_direction_id) ? direction_id - prev_direction_id : 6 - (direction_id - prev_direction_id) ;
    else
      num_evaluated += replacements.size(); //not used

    cout<<replacements.size();

    for( const auto & test_replacement : replacements ) {
      //cout << "Evaluating: " << test_replacement.str()<<endl;
      if ( eval_cache_.find( test_replacement ) == eval_cache_.end() ) {
        /* need to fire off a new thread to evaluate */
        scores.emplace_back( test_replacement,
           async( launch::async, [] ( const Evaluator & e,
                    const Whisker & r,
                    const WhiskerTree & rat ) {
              WhiskerTree replaced_whiskertree( rat );
              const bool found_replacement __attribute((unused)) = replaced_whiskertree.replace( r );
              assert( found_replacement );
              return make_pair( true, e.score( replaced_whiskertree ).score ); },
            eval_, test_replacement, rat_ ) );
      } else {
        /* we already know the score */
        //cout << "Cached: " << test_replacement.str();
        scores.emplace_back( test_replacement,
           async( launch::deferred, [] ( const double value ) {
               return make_pair( false, value ); }, eval_cache_.at( test_replacement ) ) );
      }
    }

    for (auto & x : scores) { // wait for all threads to finish
        try{
          x.second.wait();
        } catch (const std::future_error& e) {
          cout << "Caught a future_error with code \"" << e.code()
                  << "\"\nMessage: \"" << e.what() << "\"\n";
        }
    }

    /* find the best one */
    //for ( auto & x : scores ) {
    for( uint i = 0; i < scores.size(); i++ ){ // not the most maintainable code. Refer below
      if( not scores[i].second.valid() ){
        cout << "Element " << i << " is not valid: " << scores[i].first.str() << endl ;
        //exit( 1 );
      }
      const auto & replacement( scores[i].first );
      const auto outcome( scores[i].second.get() );
      const bool was_new_evaluation( outcome.first );
      const double score( outcome.second );

      //cout << "Score observed: " << score << endl;

      /* should we cache this result? */
      try{
        if ( was_new_evaluation ) {
          eval_cache_.insert( make_pair( replacement, score ) );
        }
      } catch (const std::bad_alloc& e) {
        cerr << "Exception: " << e.what() << endl;
        cout << replacement.str();
        continue;
        //auto temp __attribute((unused)) = make_pair( replacement, score );
        //exit( 1 );
      }

      if ( score > score_to_beat_ ) {
        //cout << " Got a better action! " << endl;
        score_to_beat_ = score;
        whisker_to_improve = replacement;
        if ( search_mode == 0 )
          direction_id = (direction_id + i) % 6; // not maintainable because of this
      }
    }

    cerr << "Score jumps from " << prev_score << " to " << score_to_beat_ << endl;
    cout << "Chosen Whisker: " << whisker_to_improve.str() << endl;

    if ( score_to_beat_ <= prev_score ){
      assert( score_to_beat_ == prev_score );

      if ( search_mode == 0 and num_evaluated < 6 ) {
        scores.clear();
        continue;
      }

      if ( search_mode == 0 )
        return score_to_beat_;
      else{
        search_mode = 0;
        num_evaluated = 0;
      }
    }
    else {
      prev_score = score_to_beat_;
      //search_mode ++;
      //if ( i == scores.size() - 1 )
      //  search_mode += scores.size();
      //else
        search_mode = 1;
    }

    scores.clear();
  }
}
