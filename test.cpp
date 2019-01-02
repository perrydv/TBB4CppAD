// This file must be compiled with include paths for both cppad and TBB.
// It must be linked against tbb or tbb_debug with paths as needed.
#include "cppad/cppad.hpp"
#include <vector>
#include "TBB4CppAD.h"

#define TEST_VERBOSE

/* Simple operation for CppAD taping:*/
template<class T>
T quadratic(double coef, const T &x) {
  T ans = coef * x * x;
  return ans;
}

/* Functor class for TBB parallel_for*/
class parallel_grad {
public:
  std::vector<double> &xgrid;
  std::vector<double> &gradgrid;
  TBB4CppAD::multithread_tape_manager<double> *MTMp;
  typedef TBB4CppAD::multithread_tape_manager<double>::tape_scoped_lock tape_scoped_lock;
  void operator()(const tbb::blocked_range<size_t> &r) const {
#ifdef TEST_VERBOSE
    printf("starting block from %zu to %zu\n", r.begin(), r.end());
#endif
    std::vector<double> x(1);
    std::vector<double> grad(1);
    for(size_t i = r.begin(); i != r.end(); ++i) {
#ifdef TEST_VERBOSE
      printf("Running %zu\n", i);
#endif
      x[0] = xgrid[i];
      { //lock will be released at the closing } of this local scope.
	tape_scoped_lock tape(MTMp);// Acquire tape in closest proximity to its use.
	grad = tape->Jacobian(x);   // Use tape like ADFun<double>
      }
      gradgrid[i] = grad[0];
    }
  }
  parallel_grad(std::vector<double> &xg,
		std::vector<double> &gg,
		TBB4CppAD::multithread_tape_manager<double>* MTMp_) :
    xgrid(xg),
    gradgrid(gg),
    MTMp(MTMp_) {}
};

int main(int argc, char* argv[]) {
  bool ok = true;
  /* Make tape of the function y = 3*x^2. */
  std::vector<CppAD::AD<double> > x(1);
  std::vector<CppAD::AD<double> > y(1);
  x[0] = 2.0;
  CppAD::Independent(x);
  y[0] = quadratic(3.0, x[0]);
  CppAD::ADFun<double> f(x, y);

  std::vector<double> xin(1);
  std::vector<double> grad(1);
  /* Check that the tape works in serial mode*/
  xin[0] = 3.0;
  grad = f.Jacobian(xin);
  ok &= grad[0] == 6.0 * xin[0];
  if(!ok) printf("The tape doesn't work in serial mode.");
#ifdef TEST_VERBOSE
  printf("Serial test gives %g (should be 18.0).\n", grad[0]);
#endif

  /* Make a tape manager with four copies of the tape*/
  TBB4CppAD::multithread_tape_manager<double> MTM(&f, 4);
  /* Get derivatives for every input from 1 to 100*/
  size_t n = 100;
  std::vector<double> xgrid(n);
  std::vector<double> gradgrid(n);
  for(size_t i = 0; i < n; ++i) xgrid[i] = 1.0 * i;
  parallel_for(tbb::blocked_range<size_t>(0, n),
	       parallel_grad(xgrid, gradgrid, &MTM));
#ifdef TBB4CPPAD_USE_PARALLEL_SETUP
  TBB4CppAD::multithread_tape_manager<double>::parallel_status = false;
#endif
#ifdef TEST_VERBOSE
  printf("Finished parallel_for\n");
#endif
  for(size_t i = 0; i < n; ++i) {
    ok &= gradgrid[i] == 6.0 * xgrid[i];
  }
#ifdef TEST_VERBOSE
  printf("Here are the results:\n");
  for(size_t i = 0; i < n; ++i) {
    printf("%g ==> %g\n", xgrid[i], gradgrid[i]);
  }
#endif
  if(ok) printf("Test worked\n");
  else printf("Test failed\n");
  return ok;
}
