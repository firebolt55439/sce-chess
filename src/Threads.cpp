#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "MoveGen.h"
#include "Evaluation.h"
#include "Search.h"
#include "Threads.h"

ThreadPool Threads;

void ThreadBase::notify_one(void){
	// This wakes up this thread. //
	mutex.lock(); // get the mutex
	sleep_cond.notify_one(); // signal the conditional variable
	mutex.unlock(); // let go of the mutex and let thread do work
}

void ThreadBase::wait_for(volatile const bool& cond){
	mutex.lock();
	while(!cond){
		sleep_cond.wait(mutex); // this thread will constantly sleep (and resist attempts at being woken up) until 'cond' is true
	}
	mutex.unlock();
}

void TimerThread::idle_loop(void){
	// The timer thread polls every 'PollEvery' milliseconds to
	// see if we are out of time.
	while(!exit){ // as long as we aren't dead
		mutex.lock();
		if(!exit){
			// Now put the thread to sleep. //
			sleep_cond.wait_for(mutex, (run ? PollEvery : INT_MAX)); // either poll every 'PollEvery' msec or just wait on a mutex depending on if we are running or not
		}
		mutex.unlock();
		if(run){
			Search::check_time_limit(); // this checks if the search is out of time
		}
	}
}

void MainThread::idle_loop(void){
	// The main thread does the main searching and thinking (and launches
	// all new searches).
	while(!exit){ // as long as we are alive
		mutex.lock(); // first, grab our mutex
		thinking = false; // we aren't right now, after all...
		while(!thinking && !exit){ // as long as we have nothing to do
			sleep_cond.wait(mutex); // we'll just keep waiting and resisting being woken up
		}
		mutex.unlock();
		if(!exit){
			// OK, we were signaled to wake up by getting notify_one(), but we are
			// here, meaning that the while loop above had stopped. We know
			// that we are not exiting, therefore we are now thinking.
			searching = true;
			assert(thinking);
			Search::think();
			searching = false;
		}
	}
}

extern "C" void* thread_start_func(void* th_v){
	ThreadBase* th = (ThreadBase*) th_v;
	th->idle_loop();
	return 0;
}

template<typename T>
T* new_thread(void){
	T* th = new T();
	pthread_create(th->get_handle(), NULL, thread_start_func, th); // th = parameter to thread_start_func
	return th;
}

void ThreadPool::init(void){
	timer = new_thread<TimerThread>();
	main_thread = new_thread<MainThread>();
}

void ThreadPool::start_searching(const Board& pos, const Search::SearchLimits& limits, Search::BoardStateStack& states){
	// First, wait for the main thread to finish thinking. //
	//printf("Waiting for main thread to finish thinking...\n");
	while(main_thread->thinking){
		usleep(TimerThread::PollEvery); // poll the main thread
	}
	//printf("Main thread finished!\n");
	// Now, start searching. //
	Search::SearchTime = get_system_time_msec();
	Search::Signals.stop = Search::Signals.stop_on_ponder_hit = Search::Signals.failed_low_at_root = false;
	Search::RootMoves.clear();
	Search::RootPos = pos;
	Search::Limits = limits;
	if(states.get()){ // if there's nothing, preserve current BoardStateStack
		//printf("Preserving current.\n");
		Search::SetupStates = std::move(states);
		assert(!states.get()); // we have transferred ownership above
	}
	//printf("Adding root moves...\n");
	for(MoveList<LEGAL> it(pos); *it; it++){
		if(limits.SearchMoves.empty() || std::count(limits.SearchMoves.begin(), limits.SearchMoves.end(), *it)){
			Search::RootMoves.push_back(Search::RootMove(*it));
		}
	}
	//printf("Notifying main thread...\n");
	main_thread->thinking = true;
	main_thread->notify_one(); // let's get thinking
	//printf("Done!\n");
}











































