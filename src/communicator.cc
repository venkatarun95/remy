#include "communicator.hh"

using namespace std;

/****************************************************************************/
/*************************Job Generator Communicator*************************/
/****************************************************************************/

JobGeneratorCommunicator::JobGeneratorCommunicator( int port )
  : assign_jobs_lock(),
    modifying_endpoint_connections_lock(),
    connection_acceptor_thread(),
    processor_endpoints(),
    num_active_endpoints( 0 ),
    bound_socket( 0 ),
    job_answer_locks(),
    job_results(),
    job_results_listener_thread()
{
  num_active_endpoints = 0;

  bound_socket = socket( AF_INET, SOCK_STREAM, 0 ); // for listening

  // bind to a port so that we can listen on it
  sockaddr_in addr_struct;
  memset( (char *) &addr_struct, 0, sizeof(addr_struct) );

  addr_struct.sin_family = AF_INET;
  addr_struct.sin_port = htons(port);
  addr_struct.sin_addr.s_addr = htonl(INADDR_ANY);
  if ( bind( bound_socket, (struct sockaddr*)&addr_struct, sizeof(addr_struct) ) != 0 ) {
    cerr << "Error while binding socket as server. Code: " << errno << endl;
  }

  if( listen( bound_socket, 128 ) ) {
    cerr << "Failed to listen as job generator server. Code: " << errno << endl;
  }

  // spawn thraed to listen for connections
  connection_acceptor_thread = thread( [this]{
    while( true ){
      int new_conn = accept( bound_socket, nullptr, nullptr );

      modifying_endpoint_connections_lock.lock();
      processor_endpoints.push_back( make_pair( new_conn, true ) );
      num_active_endpoints ++;
      assert( num_active_endpoints < 128-1 );
      modifying_endpoint_connections_lock.unlock();
      cout << "Found new endpoint" << endl;
    }
  } );

  listen_for_job_results(); // start listening for and processing any results that we get
}

// sends jobs in all_jobs from start_id to end_id (inclusive) to endpt
void JobGeneratorCommunicator::create_and_send_jobs( unsigned int endpt, vector< string > all_jobs, int start_id, int end_id, vector< future< string > >& job_futures ){
  static JobId job_id = 0; // to give a unique identifier to each job
  int maxlen = 0;

  char temp_buf[17];
  for( int i = start_id; i <= end_id; i++ ) {
    sprintf( temp_buf, "%-16d", job_id );
    all_jobs[i] = temp_buf + all_jobs[i];

    mutex *job_mutex = new mutex();
    job_answer_locks.insert( make_pair( job_id, job_mutex ) );
    job_mutex->lock();

    job_futures.push_back( async( launch::async, [this]( mutex* m, JobId id ){ 
      m->lock(); 
      m->unlock(); 
      delete m; // this is bad practice but safe as this is only accessed at creation and now. If this changes, then things could get ugly
      string res = job_results[ id ];
      job_results.erase( id );
      return res;
    }, job_mutex, job_id ) );

    ++ job_id;
    maxlen = max( maxlen, (int)all_jobs[i].size() + 1 );
  }

  char *res = new char[maxlen*(end_id - start_id + 1) + 32];
  sprintf( res, "%-16d%-16d", maxlen, end_id - start_id + 1 );
  memset( res+32, 0, maxlen*(end_id - start_id + 1) );

  for( int i = start_id; i <= end_id; i++ ) {
    void * tmp __attribute((unused)) = memcpy( res+32+(i-start_id)*maxlen, all_jobs[i].c_str(), (int)all_jobs[i].size() );
  }

  int num_bytes_sent = send( endpt, res, maxlen*(end_id - start_id + 1) + 32, 0 );
  assert( num_bytes_sent == maxlen*(end_id - start_id + 1) + 32 );
  delete res;
}

vector< future< string > > JobGeneratorCommunicator::assign_jobs( vector< string > jobs ){
  // only one thread calls this function at a time. Should not affect performance much as this function exits quite quickly.
  assign_jobs_lock.lock();
  while( true ){
    modifying_endpoint_connections_lock.lock();
    if( num_active_endpoints > 0 ){
      modifying_endpoint_connections_lock.unlock();
      break;
    }
    modifying_endpoint_connections_lock.unlock();
    cerr << "No processors available to process task." << endl;
    this_thread::sleep_for( std::chrono::seconds(5) );
  }
  int num_jobs_per_processor = jobs.size() / num_active_endpoints;
  vector< future< string > > job_result_futures;
  modifying_endpoint_connections_lock.lock();
  for ( unsigned int i = 0; i < processor_endpoints.size(); i++ ) {
    if ( processor_endpoints[i].second == false ) // if endpoint is known to be not active
      continue;
    if ( i == processor_endpoints.size() - 1 )
      create_and_send_jobs( processor_endpoints[i].first, jobs, int(i*num_jobs_per_processor), jobs.size() - 1, job_result_futures ); // Warning: last guy could get a lot of work
    else
      create_and_send_jobs( processor_endpoints[i].first, jobs, int(i*num_jobs_per_processor), int((i + 1)*num_jobs_per_processor) - 1, job_result_futures );
  }
  modifying_endpoint_connections_lock.unlock();

  assign_jobs_lock.unlock();
  return job_result_futures;
}

size_t JobGeneratorCommunicator::process_job_response_str( string response ) {
  if ( response.size() < 32 )
    return 0;
  JobId job_id;
  unsigned int response_size;
  sscanf( response.c_str(), "%u%u", &job_id, &response_size );
  if ( response.size() < 32 + response_size )
    return 0;

  job_results[job_id] = response.substr( 32, response_size );
  job_answer_locks[job_id]->unlock();
  return 32 + response_size;
}

void JobGeneratorCommunicator::listen_for_job_results(){
  assert( job_results_listener_thread.joinable() == false ); // make sure we have not already assigned some thread to this member
  job_results_listener_thread = thread( [this]{
    vector< pollfd > processor_poll_fds;
    unordered_map< ProcessorId, string > input_buffers;
    unsigned int num_endpoints_considered_for_polling = 0; // to track when to refresh processor_poll_fds
    
    char buffer[256];
    while( true ) {
      modifying_endpoint_connections_lock.lock();
      if( num_endpoints_considered_for_polling != processor_endpoints.size() ) { //Warning: in some special circumstances, some endpoints will never be listened to. To ensure this doesn't happen NEVER delete anything from processor_endpoints
        processor_poll_fds.clear();
        for( auto & x : processor_endpoints ) {
          if( x.second == true ) {
            processor_poll_fds.push_back( {(int)x.first, POLLIN, 0} );
          }
        }
        num_endpoints_considered_for_polling = processor_endpoints.size();
      }
      modifying_endpoint_connections_lock.unlock();

      int poll_result = poll( processor_poll_fds.data(), processor_poll_fds.size(), 5000 );
      if ( poll_result < 0 ) {
        cerr << "Error while polling. Code: " << errno;
      }
      if( poll_result == 0 ) // so that we can check of if more endpoints have been discovered
        continue;

      for ( auto & x : processor_poll_fds ) {
        if ( x.revents & POLLIN ) {
          int len_read = read( x.fd, buffer, 256-1 );
          input_buffers[x.fd].append( buffer, len_read );

          while ( true ) {
            int len_to_delete = process_job_response_str( input_buffers[x.fd] );
            if ( len_to_delete == 0 ) 
              break;
            input_buffers[x.fd] = input_buffers[x.fd].substr( len_to_delete );
          }
        }
      }

    }
  });
}


/****************************************************************************/
/*************************Job Processor Communicator*************************/
/****************************************************************************/

JobProcessorCommunicator::JobProcessorCommunicator( int server_port, string server_ip, function< string( string ) > s_processor_function)
  : bound_socket( 0 ),
    socket_lock( ),
    processor_function( s_processor_function ),
    num_processing_threads( 0 ),
    processing_threads_lock( )
 {
  bound_socket = socket( AF_INET, SOCK_STREAM, 0 ); // for listening

  // bind to a port so that we can listen on it
  sockaddr_in addr_struct;
  memset( (char *) &addr_struct, 0, sizeof(addr_struct) );

  addr_struct.sin_family = AF_INET;
  addr_struct.sin_port = htons( server_port );
  addr_struct.sin_addr.s_addr = inet_addr( server_ip.c_str() );
  while( connect( bound_socket, (struct sockaddr*)& addr_struct, sizeof( addr_struct ) ) < 0 ) {
    cerr << "Could not connect to job generator server. Code: " << errno << ". Trying again in 5 sec..." << endl;
    this_thread::sleep_for( chrono::seconds( 5 ) );
  }
}

void JobProcessorCommunicator::start_listener_loop() {
  const int buffer_size = 4096;
  char buffer_base[ buffer_size ], *buffer = buffer_base;
  // represents number of unprocessed bytes in buffer in units of sizeof(char)
  int buffer_remaining = 0;

  // represents the state of the listner loop's finite automata
  //  0 - expecting the beginning of an array of jobs
  //  1 - expecting the beginning of a job
  int state = 0; 

  int num_jobs_remaining = 0, max_job_len = -1; // these are valid only when state > 0
  bool fill_buffer = false; // set when more data is required before processing can take place

  char *job_buffer = new char[1];
  int job_buffer_size = 1;
  string tmp_job_res, tmp_job_str;

  // key: job_id, value: pair(job's thread, is running?)
  unordered_map< int, pair< thread, bool > > job_threads; // only so that the threads objects are not immediately destroyed

  while( true ) {
    if ( buffer_remaining == 0 ){
      pollfd fds[1] = {{ bound_socket, POLLIN, 0 }};
      poll( fds, 1, -1 ); // we poll so that threads can write to the socket while we are waiting for data
      assert( fds[0].revents & POLLIN );
      
      socket_lock.lock(); // now the socket is locked, so worker threads cannot write back their results
      buffer_remaining = read( bound_socket, buffer_base, buffer_size ); //get more data
      socket_lock.unlock();
      buffer = buffer_base;
      fill_buffer = false;
    }
    else if ( fill_buffer ){
      pollfd fds[1] = {{ bound_socket, POLLIN, 0 }};
      poll( fds, 1, -1 ); // we poll so that threads can write to the socket while we are waiting for data
      assert( fds[0].revents & POLLIN );

      socket_lock.lock(); // now the socket is locked, so worker threads cannot write back their results
      buffer_remaining += read( bound_socket, buffer, buffer_size - buffer_remaining );
      socket_lock.unlock();
      fill_buffer = false;
    }

    switch ( state ){
      case 0:
        if ( buffer_remaining <= 32 ){
          fill_buffer = true;
          continue;
        }
        sscanf( buffer, "%d%d", &max_job_len, &num_jobs_remaining );

        if ( job_buffer_size < max_job_len ){
          delete job_buffer;
          job_buffer = new char[max_job_len+1];
          job_buffer_size = max_job_len;
        }

        buffer_remaining -= 32;
        buffer += 32;
        state = 1;
      break;

      case 1:
        // ensure the buffer contains atleast one job
        if ( buffer_remaining < max_job_len ){
          fill_buffer = true;
          continue;
        }

        // block until some of the threads become inactive
        while ( true ){
          processing_threads_lock.lock();
          if( job_threads.size() > thread::hardware_concurrency() ){ //clean 'em up
            for( auto & x : job_threads ){
              if( x.second.second == false ){
                x.second.first.join(); //so that we don't kill it while it is still running (perhaps finishing destructors)
                job_threads.erase( x.first );
              }
            }
          }
          if( num_processing_threads <= thread::hardware_concurrency() ){
            processing_threads_lock.unlock();
            break;
          }
          processing_threads_lock.unlock();    
          this_thread::sleep_for( chrono::seconds(1) );
        }

        int job_id;
        sscanf( buffer, "%d", &job_id );
        memcpy( job_buffer, buffer + 16, max_job_len );
        job_buffer[max_job_len] = '\0';
        tmp_job_str = job_buffer; // indivudual jobs are null terminated
        
        processing_threads_lock.lock();
        // this thread calls the processor_function, cleans up and sends the result back to the server.
        job_threads.insert( make_pair( job_id, make_pair( 
          thread( [this, &job_threads](int job_id, string job_str){
            string res = processor_function( job_str );

            char temp_buf[33];
            sprintf( temp_buf, "%-16d%-16d", job_id, (int)res.size() );
            res = temp_buf + res;
            socket_lock.lock();
            send( bound_socket, res.c_str(), res.size(), 0 );
            socket_lock.unlock();
            
            lock_guard< mutex > lock( processing_threads_lock );
            num_processing_threads --;
            job_threads[job_id].second = false;
          }, job_id, tmp_job_str ), true) ) );
        ++ num_processing_threads;
        processing_threads_lock.unlock();

        buffer_remaining -= max_job_len;
        buffer += max_job_len;
        -- num_jobs_remaining;
        if ( num_jobs_remaining == 0 )
          state = 0;
        else
          state = 1;
      break;
      default:
        assert( false );
    }
  }
}