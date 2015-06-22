#include <cassert>
#include <cmath>
#include <algorithm>
#include <boost/functional/hash.hpp>

#include "whisker.hh"

using namespace std;

vector< Whisker > Whisker::bisect( void ) const
{
  vector< Whisker > ret;
  for ( auto &x : _domain.bisect() ) {
    Whisker new_whisker( *this );
    new_whisker._domain = x;
    ret.push_back( new_whisker );
  }
  return ret;
}

Whisker::Whisker( const unsigned int s_window_increment, const double s_window_multiple, const double s_intersend, const MemoryRange & s_domain )
  : _generation( 0 ),
    _window_increment( s_window_increment ),
    _window_multiple( s_window_multiple ),
    _intersend( s_intersend ),
    _domain( s_domain )
{
}

Whisker::Whisker( const Whisker & other )
  : _generation( other._generation ),
    _window_increment( other._window_increment ),
    _window_multiple( other._window_multiple ),
    _intersend( other._intersend ),
    _domain( other._domain )
{
}

template < typename T >
bool Whisker::OptimizationSetting< T >::eligible_value( const T & value ) const
{
  return value >= min_value and value <= max_value;
}

template < typename T >
vector< T > Whisker::OptimizationSetting< T >::alternatives( const T & value, int num_alternatives, int change_direction ) const
{
  assert( eligible_value( value ) );

  vector< T > ret;//( 1, value );

  for ( T proposed_change = min_change;
	proposed_change <= max_change;
	proposed_change *= multiplier ) {
    /* explore positive change */
    const T proposed_value = ( value + proposed_change * change_direction );
    
    if ( eligible_value( proposed_value ) ) {
      ret.push_back( proposed_value );
    }

    if ( ret.size() >= num_alternatives )
        break;
  }

  return ret;
}

vector< Whisker >  Whisker::next_generation( int& cur_direction_id, int search_mode, int num_alternatives ) const
{
  // if search_mode = 0, return values exploring various directions
  // else return values exploring one direction in a geometric manner, starting at the power of search_mode's value
  vector< Whisker > ret;

  if ( search_mode == 0 ){
    int init_direction_id = cur_direction_id;
    for ( int i = 0;i < num_alternatives; i++ ){
      Whisker new_whisker { *this };
      new_whisker._generation++;

      int change_direction = ( cur_direction_id >= 3 ) ? 1 : -1;

      switch ( cur_direction_id % 3 ){
        case 0:
          new_whisker._intersend += change_direction * get_optimizer().intersend.min_change;
        break;
        case 1:
          new_whisker._window_multiple += change_direction * get_optimizer().window_multiple.min_change;
        break;
        case 2:
          new_whisker._window_increment += change_direction * get_optimizer().window_increment.min_change;
        break;
        default:
          assert( false );
      }

      new_whisker.round();
      cur_direction_id = ( cur_direction_id + 1) % 6;

      if( !( get_optimizer().intersend.eligible_value( new_whisker._intersend ) &&
              get_optimizer().window_multiple.eligible_value( new_whisker._window_multiple ) &&
              get_optimizer().window_increment.eligible_value( new_whisker._window_increment ) ) ){
        i --;
        continue;
      }

      ret.push_back( new_whisker );


      if( init_direction_id == cur_direction_id )
        break;
    }

    assert( ret.size() >= 3 );
  }
  else{
    int change_direction = ( cur_direction_id >= 3 ) ? 1 : -1;
    switch ( cur_direction_id % 3 ){
      case 0:
        for ( const auto & alt : get_optimizer().intersend.alternatives( _intersend, num_alternatives, change_direction ) ){
          Whisker new_whisker { *this };
          new_whisker._generation++;
          new_whisker._intersend = alt;
          new_whisker.round();
          ret.push_back( new_whisker );
        }
      break;
      case 1:
        for ( const auto & alt : get_optimizer().window_multiple.alternatives( _window_multiple, num_alternatives, change_direction ) ){
          Whisker new_whisker { *this };
          new_whisker._generation++;
          new_whisker._window_multiple = alt;
          new_whisker.round();
          ret.push_back( new_whisker );
        }
      break;
      case 2:
        for ( const auto & alt : get_optimizer().window_increment.alternatives( _window_increment, num_alternatives, change_direction ) ){
          Whisker new_whisker { *this };
          new_whisker._generation++;
          new_whisker._window_increment = alt;
          new_whisker.round();
          ret.push_back( new_whisker );
        }
      break;
      default:
        assert( false );
    }
  }


  return ret;
}

void Whisker::promote( const unsigned int generation )
{
  _generation = max( _generation, generation );
}

string Whisker::str( const unsigned int total ) const
{
  char tmp[ 256 ];
  snprintf( tmp, 256, "{%s} gen=%u usage=%.4f => (win=%d + %f * win, intersend=%f)",
	    _domain.str().c_str(), _generation, double( _domain.count() ) / double( total ), _window_increment, _window_multiple, _intersend );
  return tmp;
}

RemyBuffers::Whisker Whisker::DNA( void ) const
{
  RemyBuffers::Whisker ret;

  ret.set_window_increment( _window_increment );
  ret.set_window_multiple( _window_multiple );
  ret.set_intersend( _intersend );
  ret.mutable_domain()->CopyFrom( _domain.DNA() );

  return ret;
}

Whisker::Whisker( const RemyBuffers::Whisker & dna )
  : _generation( 0 ),
    _window_increment( dna.window_increment() ),
    _window_multiple( dna.window_multiple() ),
    _intersend( dna.intersend() ),
    _domain( dna.domain() )
{
}

void Whisker::round( void )
{
  _window_multiple = (1.0/10000.0) * int( 10000 * _window_multiple );
  _intersend = (1.0/10000.0) * int( 10000 * _intersend );
}

size_t hash_value( const Whisker & whisker )
{
  size_t seed = 0;
  boost::hash_combine( seed, whisker._window_increment );
  boost::hash_combine( seed, whisker._window_multiple );
  boost::hash_combine( seed, whisker._intersend );
  boost::hash_combine( seed, whisker._domain );

  return seed;
}
