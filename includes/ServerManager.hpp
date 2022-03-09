#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include <unistd.h>
#include <sys/time.h>
#include <iostream>
#include "Server.hpp"
#include "Client.hpp"
#include "Response.hpp"

class ServerManager
{
private:
	std::vector<Server> servers;
	std::vector<Client> clients;
	int max_fd;
	fd_set reads;
	struct timeval timeout;
	FILE *fp;

	std::map<int, std::string> status_info;

	ServerManager();

public:
	ServerManager(std::vector<Server> servers);
	~ServerManager();

	void create_servers();
	void wait_on_clients();
	void accept_sockets();
	void close_servers();

	void drop_client(Client client);
	void send_response();
	void send_error_page(int code, Client &Client);

	void get_method(Client &client);
	void post_method(Client &client);
	void delete_method(Client &client);

	void get_contents_list();
	void get_content();
	void get_index_page(Client &client, const char *path);
	void post_content();
	void delete_content();

	const char *find_content_type(const char *path);
	std::string find_path_in_root(const char *path, Client &client);
};

#endif