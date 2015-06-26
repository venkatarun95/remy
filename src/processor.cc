#include <string>
#include <thread>

#include "communicator.hh"
#include "evaluator.hh"

using namespace std;

int main(){
  cout << thread::hardware_concurrency() << " threads can be used." << endl;
  JobProcessorCommunicator proc(6001, "127.0.0.1", 
    [](string job){
      cout << "Got a job!" << endl << flush;
      ProblemBuffers::Problem problem;
      problem.ParseFromString( job );
      AnswerBuffers::Result result = Evaluator::parse_problem_and_evaluate( problem );
      string res_str;
      result.SerializeToString( &res_str );
      return res_str;
  });

  //thread temp = thread( [&proc]{ proc.start_listener_loop(); } );
  proc.start_listener_loop();
}