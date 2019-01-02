# TBB4CppAD
Simplify use of CppAD objects with TBB multi-threaded programming

## Background
CppAD ![https://github.com/coin-or/CppAD](https://github.com/coin-or/CppAD) is a library for automatic differentiation.  It works by using overloaded data types for which mathematical operations are recorded in a "tape" that can then be re-used to compute function values or derivatives.

Threading Building Blocks (TBB) ![https://www.threadingbuildingblocks.org/](https://www.threadingbuildingblocks.org/) is a widely used multi-threading library from Intel.

CppAD and TBB do not naturally play well together.

CppAD provides an interface for multi-threaded execution that  assumes the programmer will be explicitly managing threads.  TBB, on the other hand, abstracts away thread management.   It relies on functors for parallelized tasks.  Its task manager creates and destroys functor instances as needed.

Attempting to use CppAD tapes naively in TBB functors leads to CppAD errors relating to memory management.  While it may or may not be possible to track down and resolve these errors, it should also be noted that CppAD tapes can be large, so extensive copying and deleting could be costly.

## Approach
TBB4CppAD is very small library that supports use of CppAD tapes with TBB.

The approach taken is as follows:

- A template class multithread\_tape\_manager is provided to manage a pre-determined number of copies of a CppAD tape.
- A  multithread\_tape\_manager object makes copies of the CppAD tape copies during serial execution.
- In functors for TBB multi-thread tasks, code must request and then release a tape from the multithread\_tape\_manager.
- A tape\_scoped\_lock class is provided to make acquiring and releasing a tape simpler and safer: simpler because the acquisition can be made by object instantiation and safer because release can be done by object destruction, namely when it goes out of scope.  This is all similar to scoped\_lock objects in TBB.
- Internally, the multithread\_tape\_manager uses a TBB concurrent\_bounded\_queue to loan and collect tapes.  This TBB container features a pop operation that will wait for a resource.  If all tapes are in use, it will wait for one to be returned and pushed back into the queue.

## Usage
Say one has a `CppAD::ADFun<double>` object, i.e. a tape.

1. Prior to calling a TBB multi-thread function such as `parallel_for`, make a `multithread_tape_manager<double>`, such as:

```{C++}
/* f is the CppAD::ADFun<double> object.  We indicate 4 copies should be made and managed.*/

TBB4CppAD::multithread_tape_manager<double> MTM(&f, 4);
```

2. In the class to be used by `parallel_for` (or related function):

a. Make a member variable of type `TBB4CppAD::multithread\_tape\_manager<double>*`.  Include a constructor argument to initialize this (from `&MTM`).

b. Use a tape in `operator()` as follows (taken from test.cpp):

```{C++}
{            /*lock will be released at the closing } of this local scope.*/
    TBB4CppAD::multithread_tape_manager<double>::tape_scoped_lock tape(MTMp);// Acquire tape in closest proximity to its use.
	grad = tape->Jacobian(x);       // Use tape like ADFun<double>
}
```


See test.cpp for further details.

## Efficiency caution
The approach taken in TBB4CppAD does not necessarily optimize performance.  In fact, use of CppAD tapes could become a bottleneck to TBB's performance because threads will have to wait for access to a limited number of tapes.  On the other hand, if the number of tapes matches the number of processor cores available, then it may not be much of a bottleneck.  If multiple large CppAD tapes are involved, and a multithread\_tape\_manager is used for each one, each with many copies, there could be non-trivial memory use.

## Limitations and questions
It does not appear that CppAD's multi-thread management system is necessary for the use cases presented by TBB4CppAD.   This is to be confirmed.

Use of multiple multithread\_tape\_manager objects has not been tested.  How it should work depends on the answer to the above issue.
