#ifndef REQUEST_HPP
#define REQUEST_HPP

#include "Utils.hpp"
#include "DataStructs.hpp"

class Request
{
private:
	int client_fd;
public:
	std::string method;
	std::string path;
	std::map<std::string, std::string> headers;
	std::string body;
	Request(int client_fd);
	~Request();

	bool parsing(std::string request);
	std::string get_path(); // url after program name and a slash
	std::string get_query(); // query string : after ?
	int get_client_fd();
	std::string get_port();
	friend std::ostream &operator<<(std::ostream &out, const Request &req) {
		out << "method: " << req.method << "\npath: " << req.path << "\nheaders: "
		<< req.headers << "\nbody: " << req.body << "\n";
		return out;
}
};



#endif