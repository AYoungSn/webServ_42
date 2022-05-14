#include "../includes/Request.hpp"

Request::Request(int client_fd)
{
	client_fd = client_fd;
}

Request::~Request()
{
}

int Request::get_client_fd() {
	return client_fd;
}

std::string Request::get_port() {
	int i = headers["Host"].find_first_of(":", 0);
	return headers["Host"].substr(i + 1, headers["Host"].size() - i - 1);
}

bool Request::parsing(std::string request)
{
	std::cout << "> request parsing" << std::endl;
	int i = request.find_first_of(" ", 0);
	method = request.substr(0, i);
	int j = request.find_first_of(" ", i + 1);
	path = request.substr(i + 1, j - i - 1);
	headers["HTTP"] = request.substr(j + 1, request.find_first_of("\r", i) - j - 1);
	if (headers["HTTP"] != "HTTP/1.1")
		return false; 
	i = request.find_first_of("\n", j) + 1;
	while (i < request.size())
	{
		if (request[i] == '\r' && request[i + 1] == '\n')
		{
			this->body = request.substr(i + 2, request.size());
			break;
		}
		int deli = request.find_first_of(":", i);
		int end = request.find_first_of("\r\n", deli);
		headers[request.substr(i, deli - i)] = request.substr(deli + 2, end + 2 - deli - 3);
		i = end + 2;
	}
	return true;
}

std::string Request::get_path()
{
	int i = path.find_first_of("?", 0);
	if (i == std::string::npos)
		return path;
	return path.substr(0, i - 1);
}

std::string Request::get_query()
{
	int i = path.find_first_of("?", 0);
	if (i == std::string::npos)
		return "";
	return path.substr(i, path.size() - i);
}