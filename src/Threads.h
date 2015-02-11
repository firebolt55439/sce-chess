#ifndef THREADS_INC
#define THREADS_INC

#include "Common.h"
#include "Bitboards.h"
#include "Board.h"
#include "Search.h"

struct Mutex {
	/* A simple wrapper around a mutex. */
	private:
		pthread_mutex_t mutex;
	public:
		Mutex(void){
			pthread_mutex_init(&mutex, NULL);
		}
		
		void lock(void){
			pthread_mutex_lock(&mutex);
		}
		
		void try_lock(void){
			pthread_mutex_trylock(&mutex);
		}
		
		void unlock(void){
			pthread_mutex_unlock(&mutex);
		}
		
		pthread_mutex_t* get_handle(void){
			return &mutex;
		}
		
		~Mutex(void){
			// According to http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_mutex_destroy.html
			// trying to destroy a locked mutex results in undefined behavior.
			pthread_mutex_trylock(&mutex); // non-blocking call to lock it, whether locked or not (if locked, returns EBUSY)
			pthread_mutex_unlock(&mutex); // now unlock it properly
			pthread_mutex_destroy(&mutex); // and destroy it
		}
};

struct ConditionVariable {
	/* And a wrapper around a condition variable. */
	private:
		pthread_cond_t cond;
	public:
		ConditionVariable(void){
			pthread_cond_init(&cond, NULL);
		}
		
		~ConditionVariable(void){
			pthread_cond_destroy(&cond);
		}
		
		void wait(Mutex& mut){
			pthread_cond_wait(&cond, mut.get_handle());
		}
		
		void wait_for(Mutex& mut, int msec){
			// With timeout.
			timespec ts, *tm = &ts;
			uint64_t ms = get_system_time_msec() + msec;
			ts.tv_sec = ms / 1000;
			ts.tv_nsec = (ms % 1000) * 1000000LL;
			pthread_cond_timedwait(&cond, mut.get_handle(), tm);
		}
		
		void notify_one(void){
			pthread_cond_signal(&cond);
		}
};

struct ThreadBase {
	pthread_t thread_handle; // the thread handle for init'ing, etc.
	Mutex mutex; // mutex for data access
	ConditionVariable sleep_cond; // for sleeping/waking up by signalling
	volatile bool exit; // whether we are done or not
	
	ThreadBase(void) : exit(false) {}
	virtual ~ThreadBase(void){}
	virtual void idle_loop(void) = 0; // has to be overridden
	void notify_one(void); // wake up this thread
	void wait_for(volatile const bool& b); // wait on a volatile boolean condition
	pthread_t* get_handle(void){
		return &thread_handle;
	}
};

struct MainThread : public ThreadBase {
	volatile bool thinking; // whether the thread is thinking or not
	volatile bool searching;
	
	MainThread(void) : thinking(true) {}
	virtual void idle_loop(void);
};

struct TimerThread : public ThreadBase {
	static const int PollEvery = 5; // how often to poll, in milliseconds
	bool run;
	
	TimerThread(void) : run(false) {}
	virtual void idle_loop(void);
};

struct ThreadPool {
	MainThread* main_thread;
	TimerThread* timer;
	
	void init(void);
	void start_searching(const Board& pos, const Search::SearchLimits& limits, Search::BoardStateStack& states);
};

extern ThreadPool Threads;

#endif // #ifndef THREADS_INC