%module(directors="1", allprotected="1") uscxmlNativeJava

// import swig typemaps
//%include <arrays_java.i>
//%include <inttypes.i>
%include <boost_shared_ptr.i>

// disable warning related to unknown base class
#pragma SWIG nowarn=401
//%ignore boost::enable_shared_from_this;

%javaconst(1);

# %shared_ptr(uscxml::dom::Element);
# %shared_ptr(uscxml::dom::Executable);

%rename(equals) operator==; 
%rename(isValid) operator bool;
%ignore operator!=;
%ignore operator<;
%ignore operator=;

//**************************************************
// This ends up in the generated wrapper code
//**************************************************

%{

#include "../../../uscxml/Message.h"
#include "../../../uscxml/Interpreter.h"

using namespace uscxml;

%}

%rename(toString) operator<<;

%ignore uscxml::NumAttr;
%ignore uscxml::SCXMLParser;
%ignore uscxml::InterpreterImpl;

//***********************************************
// Parse the header file to generate wrappers
//***********************************************

%include "../../../uscxml/Message.h"
%include "../../../uscxml/Interpreter.h"

