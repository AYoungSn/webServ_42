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

#define CGI_READ_BUFFER_SIZE 64000

class CgiHandler;

class ServerManager
{
private:
	std::vector<Server> _servers;
	std::map<std::string, Server*> servers;
	std::map<std::string, Server*> default_servers;
	std::vector<Client> clients;
	int		max_fd;

	std::map<int, std::string> status_info;

	ServerManager();

public:
	fd_set	writes;
	fd_set	reads;
	
	ServerManager(std::vector<Server> servers);
	~ServerManager();

	void	wait_to_client();
	void	accept_sockets();
	bool	drop_client_or_not(Client client, bool is_alive = false);

	void	create_servers();
	void	close_servers();

	void	treat_request();
	void	print_servers_info();

	void	add_fd_selectPoll(int fd, fd_set* fds);
	void	run_selectPoll(fd_set *reads, fd_set *writes);

private:
	void	get_method(Client &client, std::string path);
	void	post_method(Client &client, Request &request);
	void	delete_method(Client &client, std::string path);

	void	send_autoindex_page(Client &client, std::string path);
	void	send_redirection(Client &client, std::string request_method);
	void	send_error_page(int code, Client &Client, std::vector<MethodType> *allow_methods = NULL);

	void	handle_cgi_GET_response(Response& res, std::string& cgi_ret, Client &client);
	void	handle_cgi_POST_response(Response& res, std::string& cgi_ret, Client &client, Request& request);
	int		send_cgi_response(Client& client, CgiHandler& ch, Request& req);
	
	/*
	** ServerManager_helper
	*/

	bool	is_allowed_method(std::vector<MethodType> allow_methods, std::string method);
	bool	is_request_done(char *request);
	bool	is_alive_request(char *request) const;
	bool	is_alive_request(std::map<std::string, std::string> *headers) const;
	bool	is_response_timeout(Client& client);
	bool	is_loc_check(std::string path, Client &client);
	bool	is_cgi(Request *request, Location *loc);

	std::string	methodtype_to_s(MethodType method) const;

	const char	*find_content_type(const char *path) const;
	std::string	find_path_in_root(std::string path, Client &client) const;

	std::string	read_with_timeout(int fd, int timeout_ms);
	std::string	get_status_cgi(std::string& cgi_ret);

	int		write_file_in_path(Client &client, std::string content, std::string path);
};

#endif