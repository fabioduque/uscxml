#define protected public
#include "uscxml/Interpreter.h"
#undef protected

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Expected path to test-predicates.scxml" << std::endl;
		exit(EXIT_FAILURE);
	}

	using namespace uscxml;
	using namespace Arabica::DOM;
	using namespace Arabica::XPath;

	Interpreter interpreter = Interpreter::fromURI(argv[1]);
	assert(interpreter);

	Node<std::string> atomicState = interpreter.getState("atomic");
	assert(Interpreter::isAtomic(atomicState));
	assert(!Interpreter::isParallel(atomicState));
	assert(!Interpreter::isCompound(atomicState));

	Node<std::string> compoundState = interpreter.getState("compound");
	assert(!Interpreter::isAtomic(compoundState));
	assert(!Interpreter::isParallel(compoundState));
	assert(Interpreter::isCompound(compoundState));

	Node<std::string> parallelState = interpreter.getState("parallel");
	assert(!Interpreter::isAtomic(parallelState));
	assert(Interpreter::isParallel(parallelState));
	assert(!Interpreter::isCompound(parallelState)); // parallel states are not compound!

	NodeSet<std::string> initialState = interpreter.getInitialStates();
	assert(initialState[0] == atomicState);

	NodeSet<std::string> childs = interpreter.getChildStates(compoundState);
	Node<std::string> compundChild1 = interpreter.getState("compundChild1");
	Node<std::string> compundChild2 = interpreter.getState("compundChild2");
	assert(childs.size() > 0);
	assert(Interpreter::isMember(compundChild1, childs));
	assert(Interpreter::isMember(compundChild2, childs));
	assert(!Interpreter::isMember(compoundState, childs));

	assert(Interpreter::isDescendant(compundChild1, compoundState));

	std::string transEvents;
	transEvents = "error";
	assert(InterpreterImpl::nameMatch(transEvents, "error"));
	assert(!InterpreterImpl::nameMatch(transEvents, "foo"));

	transEvents = "error foo";
	assert(InterpreterImpl::nameMatch(transEvents, "error"));
	assert(InterpreterImpl::nameMatch(transEvents, "error.send"));
	assert(InterpreterImpl::nameMatch(transEvents, "error.send.failed"));
	assert(InterpreterImpl::nameMatch(transEvents, "foo"));
	assert(InterpreterImpl::nameMatch(transEvents, "foo.bar"));
	assert(!InterpreterImpl::nameMatch(transEvents, "errors.my.custom"));
	assert(!InterpreterImpl::nameMatch(transEvents, "errorhandler.mistake"));
	assert(!InterpreterImpl::nameMatch(transEvents, "errOr.send"));
	assert(!InterpreterImpl::nameMatch(transEvents, "foobar"));
}