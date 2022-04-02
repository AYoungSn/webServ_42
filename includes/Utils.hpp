#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <dirent.h>
#include <map>

int replace(std::string &original, std::string word1, std::string word2);
std::string dir_listing();
std::vector<std::string> split(std::string input, char delimiter);

template<typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &op)
{
	for (int i = 0; i < op.size(); i++)
	{
		out << op[i] << "(" << i << ") ";
	}
	return out;
}

template<typename T1, typename T2>
std::ostream &operator<<(std::ostream &out, const std::map<T1, T2> &op)
{
	for (typename std::map<T1, T2>::const_iterator i = op.begin(); i != op.end(); i++)
	{
		out << "[ " << (*i).first << " ] = " << (*i).second << "\n";
	}
	return out;
}

#endif