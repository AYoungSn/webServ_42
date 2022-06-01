#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include <string.h>
#include "Client.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CgiHandler.hpp"

#define CGI_READ_BUFFER_SIZE 64000

class ServerManager
{
private:
	std::vector<Server> servers;
	std::vector<Client> clients;
	int max_fd;
	fd_set reads;
	fd_set writes;

	std::map<int, std::string> status_info;

	ServerManager();

public:
	ServerManager(std::vector<Server> servers);
	~ServerManager();

	void accept_sockets();

	void wait_to_client();
	void drop_client(Client client);

	void create_servers();
	void close_servers();

	void treat_request();
	void print_servers_info();

private:
	void add_fd_selectPoll(int fd, fd_set* fds);
	void run_selectPoll(fd_set *reads, fd_set *writes);
	size_t send_cgi_response(Client& client, CgiHandler& ch);
	void send_error_page(int code, Client &Client, std::vector<MethodType> *allow_methods);
	void send_405_error_page(int code, Client &Client, std::vector<MethodType> allow_methods);
	void send_redirection(Client &client, std::string request_method);
	int	is_allowed_method(std::vector<MethodType> allow_methods, std::string method);
	std::string methodtype_to_s(MethodType method);
	bool handle_CGI(Request *request, Location *loc);
	bool is_response_timeout(Client& client);

	void get_method(Client &client, std::string path);
	void post_method(Client &client, Request &request);
	void delete_method(Client &client, std::string path);

	void get_autoindex_page(Client &client, std::string path);

	const char *find_content_type(const char *path);
	std::string find_path_in_root(std::string path, Client &client);
	std::string get_status_cgi(std::string& cgi_ret);
	void create_cgi_msg(Response& res, std::string& cgi_ret, Client &client);
	std::string read_with_timeout(int fd, int timeout_ms);
};

#endif