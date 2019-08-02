#ifndef MY_EXCEPTIONS_HPP
#define MY_EXCEPTIONS_HPP

#include <string>
#include <exception>
#include <stdexcept>

namespace Pptlib
{

using string = std::string;
using Exception = std::exception;

class UnexpectedCallException : public Exception
{
public:
	UnexpectedCallException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

class NullArgumentException : public Exception
{
public:
	NullArgumentException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

class InvalidArgumentException : public Exception
{
public:
	InvalidArgumentException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

class NotImplementedException : public Exception
{
public:
	NotImplementedException(const string& message) : Message(message) {}
	virtual const char* what()noexcept { return Message.c_str(); }
	string Message;
};

}

#endif // !MY_EXCEPTIONS_HPP
