#ifndef STRING_FORMAT_HPP
#define STRING_FORMAT_HPP

#include <string>
#include <vector>
#include <sstream>
#include "MyExceptions.hpp"

namespace Pptlib
{

using std::string;
using std::vector;
using std::ostringstream;

struct ICustomFormatter
{
	ICustomFormatter() = default;
	virtual ~ICustomFormatter() = default;
	virtual string Format(const string& formats)const = 0;
};

struct FormatStringAnalyzer
{
	static string Analysis(
		const string& resource,
		const vector<ICustomFormatter*> objects)
	{
		try
		{
			ostringstream bufStream;
			size_t index = 0;
			while (index < resource.size())
			{
				if (resource[index] == '{')
				{
					if (resource.at(index + 1) == '{')
					{
						bufStream << '{';
						++index;
					}
					else
					{
						++index;
						if (!isdigit(resource.at(index)))
							throw InvalidArgumentException(
								"Can\'t find format object index."
							);
						size_t arrIndex = 0;
						while (isdigit(resource.at(index)))
						{
							arrIndex = arrIndex * 10 + resource.at(index) - '0';
							++index;
						}
						if (resource[index] != ':' && resource[index] != '}')
							throw InvalidArgumentException(
								"Unexpected char in format string."
							);
						if (resource[index] == ':') ++index;
						ostringstream tmpBufStream;
						while (!(resource.at(index) == '}'
							&& (index + 1 == resource.size()
								|| resource.at(index + 1) != '}')))
						{
							if (resource[index] == '{' && resource.at(index + 1) != '{')
								throw InvalidArgumentException(
									"Brackets do not match."
								);
							tmpBufStream << resource[index];
							if (resource[index] == '}' || resource[index] == '{')
								++index;
							++index;
						}
						bufStream << objects.at(arrIndex)->Format(tmpBufStream.str());
					}
				}
				else
				{
					if (resource[index] == '}')
					{
						if (resource.at(++index) == '}')
							bufStream << '}';
						else
							throw InvalidArgumentException(
								"Brackets do not match."
							);
					}
					else
					{
						bufStream << resource[index];
					}
				}
				++index;
			}
			return bufStream.str();
		}
		catch (InvalidArgumentException& ex)
		{
			throw;
		}
		catch (Exception& ex)
		{
			throw InvalidArgumentException("Bad format string.");
		}
	}
};
}

#endif