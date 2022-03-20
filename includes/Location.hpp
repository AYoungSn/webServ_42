#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <vector>
#include <map>
#include "DataStructs.hpp"

class Location
{
public:
	std::string path;
	std::string root;
	std::vector<std::string> index;
	std::vector<MethodType> allow_methods;
	std::map<std::string, std::string> cgi_info;

public:
	Location();
	~Location();

	static MethodType s_to_methodtype(std::string str);
};

#endif