#include <cstdio>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ratbreeder.hh"

using namespace std;

void dump_to_file( string filename, const ConfigRange& configuration_range, const WhiskerTree & whiskers )
{
      char of[ 128 ];
      snprintf( of, 128, "%s", filename.c_str() );
      fprintf( stderr, "Writing to \"%s\"... ", of );
      int fd = open( of, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR );
      if ( fd < 0 ) {
	perror( "open" );
	exit( 1 );
      }

      auto remycc = whiskers.DNA();
      remycc.mutable_config()->CopyFrom( configuration_range.DNA() );
      remycc.mutable_optimizer()->CopyFrom( Whisker::get_optimizer().DNA() );

      if ( not remycc.SerializeToFileDescriptor( fd ) ) {
	fprintf( stderr, "Could not serialize RemyCC.\n" );
	exit( 1 );
      }

      if ( close( fd ) < 0 ) {
	perror( "close" );
	exit( 1 );
      }

      fprintf( stderr, "done.\n" );
}

int main( int argc, char *argv[] )
{
  WhiskerTree whiskers1, whiskers2;
  string output_filename;

  for ( int i = 1; i < argc; i++ ) {
    string arg( argv[ i ] );
    if ( arg.substr( 0, 3 ) == "if=" ) {
      string filename( arg.substr( 3 ) );
      int fd = open( filename.c_str(), O_RDONLY );
      if ( fd < 0 ) {
	perror( "open" );
	exit( 1 );
      }

      RemyBuffers::WhiskerTree tree;
      if ( !tree.ParseFromFileDescriptor( fd ) ) {
	fprintf( stderr, "Could not parse %s.\n", filename.c_str() );
	exit( 1 );
      }
      whiskers1 = whiskers2 = WhiskerTree( tree );

      if ( close( fd ) < 0 ) {
	perror( "close" );
	exit( 1 );
      }
    } else if ( arg.substr( 0, 3 ) == "of=" ) {
      output_filename = string( arg.substr( 3 ) );
    }
  }

  ConfigRange configuration_range;
  configuration_range.link_packets_per_ms = make_pair( 1.0, 1.0 ); /* 10 Mbps to 20 Mbps */
  configuration_range.rtt_ms = make_pair( 100, 100 ); /* ms */
  configuration_range.max_senders = 16;
  configuration_range.mean_on_duration = 1000;
  configuration_range.mean_off_duration = 1000;
  //  configuration_range.lo_only = true;
  RatBreeder breeder( configuration_range );

  unsigned int run = 0;

  printf( "#######################\n" );
  printf( "Optimizing for link packets_per_ms in [%f, %f]\n",
	  configuration_range.link_packets_per_ms.first,
	  configuration_range.link_packets_per_ms.second );
  printf( "Optimizing for rtt_ms in [%f, %f]\n",
	  configuration_range.rtt_ms.first,
	  configuration_range.rtt_ms.second );
  printf( "Optimizing for num_senders = 1-%d\n",
	  configuration_range.max_senders );
  printf( "Optimizing for mean_on_duration = %f, mean_off_duration = %f\n",
	  configuration_range.mean_on_duration, configuration_range.mean_off_duration );

  printf( "Initial rules (use if=FILENAME to read from disk): %s %s\n", whiskers1.str().c_str(), whiskers2.str().c_str() );
  printf( "#######################\n" );

  if ( !output_filename.empty() ) {
    printf( "Writing to \"%s.N\".\n", output_filename.c_str() );
  } else {
    printf( "Not saving output. Use the of=FILENAME argument to save the results.\n" );
  }

  while ( 1 ) {
    auto outcome = breeder.improve( whiskers1, whiskers2 );
    printf( "run = %u, score = %f\n", run, outcome.score );

    printf( "whiskers: %s %s\n", whiskers1.str().c_str(), whiskers2.str().c_str() );

    for ( auto &run : outcome.throughputs_delays ) {
      printf( "===\nconfig: %s\n", run.first.str().c_str() );
      for ( auto &x : run.second ) {
	printf( "sender: [tp=%f, del=%f]\n", x.first / run.first.link_ppt, x.second / run.first.delay );
      }
    }

    if ( !output_filename.empty() ) {
      dump_to_file( output_filename + "." + to_string( run ) + ".whiskertree1", configuration_range, whiskers1 );
      dump_to_file( output_filename + "." + to_string( run ) + ".whiskertree2", configuration_range, whiskers2 );
    }

    fflush( NULL );
    run++;
  }

  return 0;
}
