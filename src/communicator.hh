#ifndef COMMUNICATOR_HH
#define COMMUNICATOR_HH

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <future>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

// Communicator class to be used by the central server which distributes tasks
//
// When a set of jobs (given as strings) is given to it, it creates a bunch of 
// dummy 'future' values that are available when the computation is over 
// (perhaps in a different process). It gives an illusion of multithreading, 
// where processing can actually be distributed across several computers.
class JobGeneratorCommunicator{
public:
	typedef unsigned int JobId;
	typedef unsigned int ProcessorId;

private:
	// controls access to processor_endpoints, num_active_endpoints
	std::mutex modifying_endpoint_connections_lock;
	std::thread connection_acceptor_thread;

	// all machines that this needs to communicate with, each references by their id, which is their position in this list. The boolean value indicates the current up-status of the endpoint
	std::vector< std::pair< ProcessorId, bool > > processor_endpoints;

	int num_active_endpoints;

	int bound_socket;

	std::unordered_map< JobId, std::mutex* > job_answer_locks; // stores the answers returned by each processor
	std::unordered_map< JobId, std::string > job_results;

	std::thread job_results_listener_thread; //populated by listen_for_job_results

	void create_and_send_jobs( unsigned int endpt, std::vector< std::string > all_jobs, int start_id, int end_id, std::vector< std::future< std::string > >& job_futures );
	// Listens on the various sockets for any results of computations. If
	// available, properly passes them on to process_job_response_str
	// It spawns a new thread. It is called by the constructor
	void listen_for_job_results();
	// Takes a string of the response (which may be incomplete or even have 
	// parts of the next job response), sets the appropriate element from 
	// job_results, unlocks the appropriate elements from job_answer_locks. 
	// It returns the number of elements from the beginning of the buffer 
	// that are processed and may be deleted.
	size_t process_job_response_str( std::string response );

public:
	JobGeneratorCommunicator( std::string endpoint_list_filename );
	std::vector< std::future< std::string > > assign_jobs( std::vector< std::string > jobs );
};

class JobProcessorCommunicator{
private:
	//struct sockaddr_in server_addr;
	int bound_socket;
	// WARNING: Threads may write to the socket while the file descriptor is being polled.
	// THIS COULD CAUSE PROBLEMS!
	std::mutex socket_lock; // only one thread accesses the network at a time
	
	std::function< std::string( std::string ) > processor_function;

	unsigned int num_processing_threads;
	std::mutex processing_threads_lock; // for locking num_processing_threads and job_buffer (local to start_listener_loop)

public:
	JobProcessorCommunicator( int server_port, std::string server_ip, std::function< std::string( std::string ) > s_processor_function );
	void start_listener_loop();
};

#endif