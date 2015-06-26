#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "communicator.hh"
#include "evaluator.hh"
#include "network.hh"
#include "random.hh"
#include "rat.hh"
#include "whiskertree.hh"

using namespace std;
/*
// run_whiskers, config, prng_seed, trace, ticks_to_run
queue< tuple< WhiskerTree, NetConfig, unsigned int, bool, unsigned int > > job_queue;
//queue< Evaluator::Outcome > output_queue;
//mutex output_queue_lock;

void return_results( ){
  assert( false );
}

void thread_run_simulation( tuple< WhiskerTree, NetConfig, unsigned int, bool, unsigned int > job ){
  Evaluator::Outcome the_outcome;

  PRNG run_prng( get<2>(job) );
  Network< Rat, Rat > network1( Rat( get<0>( job ), get<3>( job ) ), run_prng, get<1>( job ) );
  network1.run_simulation( get<4>( job ) );

  the_outcome.score = network1.senders().utility();
  the_outcome.throughputs_delays.emplace_back( x, network1.senders().throughputs_delays() );
  the_outcome.used_whiskers = get<0>( job ); // WARNING: Is it okay to deque it immediately? Will it still be in memory?

  lock( output_queue_lock );
  output_queue.push( the_outcome );
  output_queue_lock.unlock();
}
*/

int main(){
  // some network initialization code

  /*int max_threads = thread::hardware_concurrency;

  queue< thread > threads; // assumption: jobs that start first end first. If this is violated, some resource wastage may be there

  while( true ) {
    while( threads.size() <= hardware_concurrency ) {
      auto job = job_queue.front();

      lock( num_threads_running_lock );
      num_threads_running ++;
      num_threads_running_lock.unlock();
      
      threads.push( thread( thread_run_simulation, job ) );
      job_queue.pop();

      if( job_queue.size() == 0 ){
        blocked_listen_for_jobs();
      }
    }

    // all available thread slots are busy
    // wait for earliest started job to finish and submit its results
    threads.front().join();
  }
  */

  //JobGeneratorCommunicator comm("hosts_list");

  JobProcessorCommunicator proc(6002, "127.0.0.1", [](string job){
    cout << "Processing: " << job << endl;
    return "Processed!";
  });

  thread temp = thread( [&proc]{ proc.start_listener_loop(); } );
  while( true ) continue;

  /*vector< string > jobs;
  jobs.push_back("Hello!");
  jobs.push_back("Hi");
  
  auto job_results = comm.assign_jobs( jobs );
  for( auto & x : job_results ){
    cout << endl << x.get() << " ||" << endl << flush;
  }

  this_thread::sleep_for( chrono::seconds(5) );

  jobs.push_back("A job");
  jobs.push_back("Next Job!");
  job_results = comm.assign_jobs( jobs );
  for( auto & x : job_results ){
    cout << endl << x.get() << " ||" << endl << flush;
  }*/
}