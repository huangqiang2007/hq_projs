%module libaipudrv
%{
#define SWIG_FILE_WITH_INIT
#include "src/high_level_api.h"
%}

%include "std_vector.i"
%include "std_string.i"

namespace std {
   %template(VecInt) vector<int>;
   %template(VecStr) vector<string>;
   %template(VecVecInt) vector< vector<int> >;
}

%include "src/high_level_api.h"