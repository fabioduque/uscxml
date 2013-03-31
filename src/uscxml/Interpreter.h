#ifndef RUNTIME_H_SQ1MBKGN
#define RUNTIME_H_SQ1MBKGN

#include "uscxml/Common.h"
#include "uscxml/URL.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <set>
#include <map>

#include <XPath/XPath.hpp>
#include <DOM/Document.hpp>
#include <io/uri.hpp>

#include <DOM/SAX2DOM/SAX2DOM.hpp>
#include <SAX/helpers/CatchErrorHandler.hpp>

#include "uscxml/concurrency/tinythread.h"
#include "uscxml/concurrency/eventqueue/DelayedEventQueue.h"
#include "uscxml/concurrency/BlockingQueue.h"
#include "uscxml/Message.h"
#include "uscxml/Factory.h"

#include "uscxml/server/InterpreterServlet.h"

namespace uscxml {

class HTTPServletInvoker;

class InterpreterMonitor {
public:
	virtual ~InterpreterMonitor() {}
	virtual void onStableConfiguration(Interpreter* interpreter) {}
	virtual void beforeCompletion(Interpreter* interpreter) {}
	virtual void afterCompletion(Interpreter* interpreter) {}
	virtual void beforeMicroStep(Interpreter* interpreter) {}
	virtual void beforeTakingTransitions(Interpreter* interpreter, const Arabica::XPath::NodeSet<std::string>& transitions) {}
	virtual void beforeEnteringStates(Interpreter* interpreter, const Arabica::XPath::NodeSet<std::string>& statesToEnter) {}
	virtual void afterEnteringStates(Interpreter* interpreter) {}
	virtual void beforeExitingStates(Interpreter* interpreter, const Arabica::XPath::NodeSet<std::string>& statesToExit) {}
	virtual void afterExitingStates(Interpreter* interpreter) {}
};

class NumAttr {
public:
	NumAttr(const std::string& str) {
		size_t valueStart = str.find_first_of("0123456789.");
		if (valueStart != std::string::npos) {
			size_t valueEnd = str.find_last_of("0123456789.");
			if (valueEnd != std::string::npos) {
				value = str.substr(valueStart, (valueEnd - valueStart) + 1);
				size_t unitStart = str.find_first_not_of(" \t", valueEnd + 1);
				if (unitStart != std::string::npos) {
					size_t unitEnd = str.find_last_of(" \t");
					if (unitEnd != std::string::npos && unitEnd > unitStart) {
						unit = str.substr(unitStart, unitEnd - unitStart);
					} else {
						unit = str.substr(unitStart, str.length() - unitStart);
					}
				}
			}
		}
	}

	std::string value;
	std::string unit;
};

class SCXMLParser : public Arabica::SAX2DOM::Parser<std::string> {
public:
	SCXMLParser(Interpreter* interpreter);
	void startPrefixMapping(const std::string& /* prefix */, const std::string& /* uri */);

	Arabica::SAX::CatchErrorHandler<std::string> _errorHandler;
	Interpreter* _interpreter;
};

class Interpreter {
public:
	enum Binding {
	    EARLY = 0,
	    LATE = 1
	};

	enum Capabilities {
	    CAN_NOTHING = 0,
	    CAN_BASIC_HTTP = 1,
	    CAN_GENERIC_HTTP = 2,
	};

	virtual ~Interpreter();

	static Interpreter* fromDOM(const Arabica::DOM::Document<std::string>& dom);
	static Interpreter* fromXML(const std::string& xml);
	static Interpreter* fromURI(const std::string& uri);
	static Interpreter* fromInputSource(Arabica::SAX::InputSource<std::string>& source);

	void start();
	static void run(void*);
	void join() {
		if (_thread != NULL) _thread->join();
	};
	bool isRunning() {
		return _running || !_done;
	}

	/// This one ought to be pure, but SWIG will generate gibberish if it is
	virtual void interpret() {};

	void addMonitor(InterpreterMonitor* monitor)             {
		_monitors.insert(monitor);
	}

	void removeMonitor(InterpreterMonitor* monitor)          {
		_monitors.erase(monitor);
	}

	void setBaseURI(std::string baseURI)                     {
		_baseURI = URL(baseURI);
	}
	URL getBaseURI()                                         {
		return _baseURI;
	}
	bool toAbsoluteURI(URL& uri);

	void setCmdLineOptions(int argc, char** argv);
	Data getCmdLineOptions() {
		return _cmdLineOptions;
	}

	InterpreterServlet* getHTTPServlet() {
		return _httpServlet;
	}

	DataModel getDataModel()                                 {
		return _dataModel;
	}
	void setParentQueue(uscxml::concurrency::BlockingQueue<SendRequest>* parentQueue) {
		_parentQueue = parentQueue;
	}
	std::string getXPathPrefix()                             {
		return _xpathPrefix;
	}
	std::string getXMLPrefix()                               {
		return _xmlNSPrefix;
	}
	Arabica::XPath::StandardNamespaceContext<std::string>& getNSContext() {
		return _nsContext;
	}
	std::string getXMLPrefixForNS(const std::string& ns)     {
		if (_nsToPrefix.find(ns) != _nsToPrefix.end())
			return _nsToPrefix[ns] + ":";
		return "";
	}

	void receive(const Event& event, bool toFront = false)   {
		if (toFront) {
			_externalQueue.push_front(event);
		} else {
			_externalQueue.push(event);
		}
	}
	void receiveInternal(const Event& event)                 {
		_internalQueue.push_back(event);
	}

	Event getCurrentEvent() {
		return _currEvent;
	}

	Arabica::XPath::NodeSet<std::string> getConfiguration()  {
		return _configuration;
	}
	void setConfiguration(const std::vector<std::string>& states) {
		_userDefinedStartConfiguration = states;
	}

	Arabica::DOM::Node<std::string> getState(const std::string& stateId);
	Arabica::XPath::NodeSet<std::string> getStates(const std::vector<std::string>& stateIds);

	Arabica::DOM::Document<std::string>& getDocument()       {
		return _document;
	}

	void setCapabilities(unsigned int capabilities)          {
		_capabilities = capabilities;
	}

	void setName(const std::string& name);
	const std::string& getName()                             {
		return _name;
	}
	const std::string& getSessionId()                        {
		return _sessionId;
	}

	bool runOnMainThread(int fps, bool blocking = true);

	static bool isMember(const Arabica::DOM::Node<std::string>& node, const Arabica::XPath::NodeSet<std::string>& set);

	void dump();
	bool hasLegalConfiguration();

	static bool isState(const Arabica::DOM::Node<std::string>& state);
	static bool isPseudoState(const Arabica::DOM::Node<std::string>& state);
	static bool isTransitionTarget(const Arabica::DOM::Node<std::string>& elem);
	static bool isTargetless(const Arabica::DOM::Node<std::string>& transition);
	static bool isAtomic(const Arabica::DOM::Node<std::string>& state);
	static bool isFinal(const Arabica::DOM::Node<std::string>& state);
	static bool isHistory(const Arabica::DOM::Node<std::string>& state);
	static bool isParallel(const Arabica::DOM::Node<std::string>& state);
	static bool isCompound(const Arabica::DOM::Node<std::string>& state);
	static bool isDescendant(const Arabica::DOM::Node<std::string>& s1, const Arabica::DOM::Node<std::string>& s2);

	static std::vector<std::string> tokenizeIdRefs(const std::string& idRefs);

	bool isInitial(const Arabica::DOM::Node<std::string>& state);
	Arabica::XPath::NodeSet<std::string> getInitialStates(Arabica::DOM::Node<std::string> state = Arabica::DOM::Node<std::string>());
	static Arabica::XPath::NodeSet<std::string> getChildStates(const Arabica::DOM::Node<std::string>& state);
	Arabica::XPath::NodeSet<std::string> getTargetStates(const Arabica::DOM::Node<std::string>& transition);
	Arabica::DOM::Node<std::string> getSourceState(const Arabica::DOM::Node<std::string>& transition);

	static Arabica::XPath::NodeSet<std::string> filterChildElements(const std::string& tagname, const Arabica::DOM::Node<std::string>& node);
	static Arabica::XPath::NodeSet<std::string> filterChildElements(const std::string& tagName, const Arabica::XPath::NodeSet<std::string>& nodeSet);
	Arabica::DOM::Node<std::string> findLCCA(const Arabica::XPath::NodeSet<std::string>& states);
	Arabica::XPath::NodeSet<std::string> getProperAncestors(const Arabica::DOM::Node<std::string>& s1, const Arabica::DOM::Node<std::string>& s2);
	static const std::string getUUID();

protected:
	Interpreter();
	void init();

	void normalize(Arabica::DOM::Element<std::string>& scxml);
	void setupIOProcessors();

	bool _stable;
	tthread::thread* _thread;
	tthread::mutex _mutex;

	URL _baseURI;
	Arabica::DOM::Document<std::string> _document;
	Arabica::DOM::Element<std::string> _scxml;
	Arabica::XPath::XPath<std::string> _xpath;
	Arabica::XPath::StandardNamespaceContext<std::string> _nsContext;
	std::string _xmlNSPrefix; // the actual prefix for elements in the xml file
	std::string _xpathPrefix; // prefix mapped for xpath, "scxml" is _xmlNSPrefix is empty but _nsURL set
	std::string _nsURL;       // ough to be "http://www.w3.org/2005/07/scxml"
	std::map<std::string, std::string> _nsToPrefix;

	bool _running;
	bool _done;
	bool _isInitialized;
	Binding _binding;
	Arabica::XPath::NodeSet<std::string> _configuration;
	Arabica::XPath::NodeSet<std::string> _statesToInvoke;
	std::vector<std::string> _userDefinedStartConfiguration;

	DataModel _dataModel;
	std::map<std::string, Arabica::XPath::NodeSet<std::string> > _historyValue;

	std::list<Event > _internalQueue;
	uscxml::concurrency::BlockingQueue<Event> _externalQueue;
	uscxml::concurrency::BlockingQueue<SendRequest>* _parentQueue;
	DelayedEventQueue* _sendQueue;

	Event _currEvent;
	InterpreterServlet* _httpServlet;
	std::set<InterpreterMonitor*> _monitors;

	static URL toBaseURI(const URL& url);

	void executeContent(const Arabica::DOM::Node<std::string>& content, bool rethrow = false);
	void executeContent(const Arabica::DOM::NodeList<std::string>& content, bool rethrow = false);
	void executeContent(const Arabica::XPath::NodeSet<std::string>& content, bool rethrow = false);

	void send(const Arabica::DOM::Node<std::string>& element);
	void invoke(const Arabica::DOM::Node<std::string>& element);
	void cancelInvoke(const Arabica::DOM::Node<std::string>& element);
	void returnDoneEvent(const Arabica::DOM::Node<std::string>& state);
	void internalDoneSend(const Arabica::DOM::Node<std::string>& state);
	static void delayedSend(void* userdata, std::string eventName);

	static bool nameMatch(const std::string& transitionEvent, const std::string& event);
	bool isWithinSameChild(const Arabica::DOM::Node<std::string>& transition);
	bool hasConditionMatch(const Arabica::DOM::Node<std::string>& conditional);
	bool isInFinalState(const Arabica::DOM::Node<std::string>& state);
	bool parentIsScxmlState(Arabica::DOM::Node<std::string> state);


	static boost::uuids::random_generator uuidGen;

	long _lastRunOnMainThread;
	std::string _name;
	std::string _sessionId;
	unsigned int _capabilities;

	Data _cmdLineOptions;

	IOProcessor getIOProcessor(const std::string& type);

	std::map<std::string, IOProcessor> _ioProcessors;
	std::map<std::string, std::pair<Interpreter*, SendRequest> > _sendIds;
	std::map<std::string, Invoker> _invokers;
	std::map<std::string, Invoker> _autoForwardees;
	std::map<Arabica::DOM::Node<std::string>, ExecutableContent> _executableContent;

	/// TODO: We need to remember to adapt them when the DOM is operated upon
	std::map<std::string, Arabica::DOM::Node<std::string> > _cachedStates;
	std::map<std::string, URL> _cachedURLs;

	friend class SCXMLParser;
	friend class USCXMLInvoker;
};

}

#endif
