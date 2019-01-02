#ifndef _TBB4CPPAD
#define _TBB4CPPAD

#define TBB4CPPAD_VERBOSE
//#define TBB4CPPAD_USE_PARALLEL_SETUP

#include "cppad/cppad.hpp"
#include "tbb/tbb.h"
#include "tbb/concurrent_queue.h"
#include <vector>

namespace TBB4CppAD {
  /* The template argument to all TBB4CppAD classes is the base type for CppAD tapes.  It is given as the first template argument to CppAD::ADFun<> and AD<>.  Typically, it will be double. */
  template<typename T>
  class multithread_tape_manager;

  /* Class tape_packet manages a pointer to a tape and an assigned thread_num */
  /* in case CppAD queries the thread_num. */
  template<typename T>
    class tape_packet {
  private:
    size_t thread_num;
    friend class multithread_tape_manager<T>;
  public:
    CppAD::ADFun<T> *tape;
    tape_packet() {};
  };


  /* Class tape_scoped_lock provides a class that will release a tape when the tape_scoped_lock object goes out of scope (is destructed). This imitates the scoped_lock idea of TBB, with the motivation that it is safer than relying on explicit release because destruction will occur even when going out of scope occurs due to an error.*/
  template<typename T>
  class tape_scoped_lock {
  public:
    multithread_tape_manager<T> *owner;
    tape_packet<T> *tp;
    CppAD::ADFun<T> *getTapePtr() {return(tp->tape);}
    void set_tape_packet(tape_packet<T> *newtp) {tp = newtp;}
    CppAD::ADFun<T>& operator*() {return(*(tp->tape));}
    CppAD::ADFun<T>* operator->() {return(tp->tape);}
  tape_scoped_lock(multithread_tape_manager<T> *o) :
    owner(o),
      tp(o->request_tape_packet())
	{};
    ~tape_scoped_lock();
  };

  /* Class multithread_tape_manager makes, lends, and collects multiple copies of a CppAD tape. */
  template<typename T>
    class multithread_tape_manager {
  private:
    std::vector<CppAD::ADFun<T> > tapes; //Vector of tapes.  Destruction will be automatic.
    std::vector<tape_packet<T> > all_tape_packets; 
    // TBBAD queue of pointers to tape_packets that have pointers to the tapes.
    tbb::concurrent_bounded_queue<tape_packet<T>* > available_tape_packets;

    // Set up tapes by making the copies.
    void setup_tapes(CppAD::ADFun<T> *tape, size_t num_tapes) {
      tapes.resize(num_tapes);
      for(size_t i = 0; i < num_tapes; ++i) {
	tapes[i] = *tape; // make copies of original tape
      }
    }

    // Set up tape packets by assigning thread numbers and tape pointers.
    void setup_tape_packets(size_t num_tapes) {
      all_tape_packets.resize(num_tapes);
      for(size_t i = 0; i < num_tapes; ++i) {
	all_tape_packets[i].thread_num = i;
	all_tape_packets[i].tape = &tapes[i];
      }
      available_tape_packets.set_capacity(num_tapes);
      for(size_t i = 0; i < num_tapes; ++i) {
	available_tape_packets.push(&all_tape_packets[i]);
      }
    }

  public:
    /* The next four items are for CppAD's parallel_setup().*/
    static bool parallel_status;
    static tbb::combinable<size_t> thread_specific_index;
    static size_t thread_num(void);
    static bool in_parallel();

    typedef tape_scoped_lock<T> tape_scoped_lock;
    
    multithread_tape_manager(CppAD::ADFun<T> *tape, size_t num_tapes) {
      setup_tapes(tape, num_tapes);
      setup_tape_packets(num_tapes);
#ifdef TBB4CPPAD_VERBOSE
      printf("tapes and tape_packets are set up.\n");
#endif
#ifdef TBB4CPPAD_USE_PARALLEL_SETUP
      CppAD::thread_alloc::parallel_setup(num_tapes, in_parallel, thread_num);
      CppAD::thread_alloc::hold_memory(true);
#ifdef TBB4CPPAD_VERBOSE
      printf("CppAD parallel_setup and hold_memory have been called.\n");
#endif
      CppAD::parallel_ad<double>(); //not clear if this is needed if we are not recording.
      parallel_status = true;
#endif
    }
    tape_packet<T>* request_tape_packet() {
#ifdef TBB4CPPAD_VERBOSE
      printf("requesting tape packet...\n");
#endif
      tape_packet<T>* ans_tape_packet;
      available_tape_packets.pop(ans_tape_packet);
#ifdef TBB4CPPAD_VERBOSE
      if(ans_tape_packet)
	printf("acquired tape_packet with thread_num %zu\n", ans_tape_packet->thread_num);
      else
	printf("failed to acquire tape_packet (this shouldn't happen!)\n");
#endif
#ifdef TBB4CPPAD_VERBOSE
      thread_specific_index.local() = ans_tape_packet->thread_num;
#endif
      return ans_tape_packet;
    }
    bool release_tape_packet(tape_packet<T> *tp) {
#ifdef TBB4CPPAD_VERBOSE
      printf("attempting to release a tape packet for thread_num %zu\n", tp->thread_num);
#endif
      available_tape_packets.push(tp);
#ifdef TBB4CPPAD_VERBOSE
      printf("done releasing tape packet\n");
#endif
      return true;
    }
  };

  template<typename T>
    bool multithread_tape_manager<T>::parallel_status;

  template<typename T>
  tbb::combinable<size_t> multithread_tape_manager<T>::thread_specific_index;
  
  template<typename T>
  size_t multithread_tape_manager<T>::thread_num(void) {
#ifdef TBB4CPPAD_VERBOSE
    printf("thread_num requested for tape with thread_num %zu\n", multithread_tape_manager<T>::thread_specific_index.local());
#endif
    return multithread_tape_manager<T>::thread_specific_index.local();
  }

  template<typename T>
  bool multithread_tape_manager<T>::in_parallel() {
#ifdef TBB4CPPAD_VERBOSE
    printf("in_parallel requested for tape with thread_num %zu\n", multithread_tape_manager<T>::thread_specific_index.local());
#endif
    return  multithread_tape_manager<T>::parallel_status;
  }

  template<typename T>
    tape_scoped_lock<T>::~tape_scoped_lock() {owner->release_tape_packet(tp);}
}

#endif
