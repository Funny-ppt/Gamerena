#ifndef COMMAND_ANALYZER_HPP
#define COMMAND_ANALYZER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "MyExceptions.hpp"

namespace Pptlib
{

using std::string;
using std::vector;
using std::unordered_map;
using std::function;

vector<string> Split(const string& resource, char delimCh)
{
	size_t begin = 0;
	size_t delim;
	vector<string> results;
	do
	{
		begin = resource.find_first_not_of(delimCh, begin);
		delim = resource.find(delimCh, begin);
		if (begin >= resource.size()) break;
		results.push_back(resource.substr(begin, delim - begin));
		begin = delim;
	} while (begin != string::npos);
	return results;
}

class CommandAnalyzer
{
public:
	using CommandType = function<void(vector<string>&)>;
	void AddCommand(const string& commandName, CommandType command)
	{
		if (Commands.count(commandName))
			throw InvalidArgumentException("Command name has been registered.");
		Commands[commandName] = command;
	}
    void Analysis(const string& commandLine)
    {
        if (commandLine[0] != '>')
            throw InvalidArgumentException("Invalid format.");
		size_t begin = commandLine.find_first_not_of(' ', 1);
		size_t delim = commandLine.find(' ', begin);
		string commandName = commandLine.substr(begin, delim - begin);
		begin = delim;
        if (commandName == "")
            throw InvalidArgumentException("CommandName can\'t be null.");
		if (!Commands.count(commandName))
			throw InvalidArgumentException(
				"Can\'t find command named \""
				+ commandName + "\".");
		Commands[commandName](Split(commandLine.substr(delim), ' '));
    }
private:
	unordered_map<string, CommandType> Commands;
};

}

#endif