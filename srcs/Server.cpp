#include "../includes/Server.hpp"

Server::Server(/* args */)
{
	client_body_limit = 1024;

	struct timeval tv;
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	if (recv_timeout.tv_sec != 0) recv_timeout = tv;
	if (send_timeout.tv_sec != 0) send_timeout = tv;
	
	autoindex = false;
	host = "";
	redirect_status = -1;
}

Server::~Server()
{
}

void Server::create_socket()
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo *bind_addr;
	for (int i = 0; i < port.size(); i++)
	{
		// memset(bind_addr, 0, sizeof(struct addrinfo*));
		getaddrinfo(host.c_str(), port[i].c_str(), &hints, &bind_addr);

		std::cout << "> Creating socket...\n";
		int socket_listen = socket(bind_addr->ai_family,
				bind_addr->ai_socktype, bind_addr->ai_protocol);
		fcntl(socket_listen, F_SETFL, O_NONBLOCK);
		if (socket_listen < 0)
		{
			fprintf(stderr, "socket() failed. (%d)\n", errno);
			exit(1);
		}
		int opt = 1;
		if (setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0)
			fprintf(stderr, "setsockopt: setsocketopt on server socket failed\n");
		std::cout << "> Binding socket to local address...\n";
		if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen))
		{
			fprintf(stderr, "bind() failed. (%d)\n", errno);
			perror("bind");
			exit(1);
		}
		freeaddrinfo(bind_addr);
		std::cout << "> Listening...\n";
		if (listen(socket_listen, 10) < 0)
		{
			fprintf(stderr, "listen() failed. (%d)\n", errno);
			exit(1);
		}
		this->listen_socket.push_back(socket_listen);
		std::cout << "> Socket successfully added!\n";
	}
}

void Server::print_server_info()
{
	std::cout << "------------------ Server Info ------------------\n";
	std::cout << "> server_name: " << server_name << "\n";
	std::cout << "> host: " << host << "\n";
	std::cout << "> port: " << port << "\n";
	if (redirect_status == -1)
	{
		std::cout << "> root: " << root << "\n";
		std::cout << "> client_body_limit: " << client_body_limit << "\n";
		std::cout << "> autoindex: " << (autoindex ? "on\n" : "off\n");
		std::cout << "> index: " << index << "\n";
		std::cout << "> allow_methods: " << allow_methods << "\n";
		for (int i = 0; i < locations.size(); i++)
		{
			locations[i].print_location_info();
		}
	}
	else
	{
		std::cout << "> redirect_status: " << redirect_status << "\n";
		std::cout << "> redirect_url: " << redirect_url << "\n";
	}
	std::cout << "-------------------------------------------------\n";
}

MethodType Server::s_to_methodtype(std::string str)
{
	if (str == "GET")
	{
		return GET;
	}
	else if (str == "POST")
	{
		return POST;
	}
	else if (str == "DELETE")
	{
		return DELETE;
	}
	return INVALID;
}

Location* Server::get_cur_location(std::string request_uri) const 
{
	if (this->locations.size() == 0)
		return NULL;

	std::vector<Location>::const_iterator res = this->locations.begin();
	unsigned long	longest = 0;
	bool is_in_root = true;

	for (std::vector<Location>::const_iterator it = this->locations.begin(); \
	it != this->locations.end(); it++) {
		std::string location_path = it->path;
		if (is_in_location(location_path, request_uri) && (longest < location_path.length()))
		{
			is_in_root = false;
			longest = location_path.length();
			res = it;
		}
	}
	if (is_in_root)
		return NULL;
	return const_cast<Location*>(&*res);
}

bool Server::is_in_location(std::string location_path, std::string request_uri) const
{
	if (request_uri.compare(0, location_path.length(), location_path) != 0)
		return false;
	if (request_uri[location_path.length()])
	{
		if (request_uri[location_path.length()] == '/' \
		|| request_uri[location_path.length()] == '?')
			return true;
		else
			return false;
	}
	else
		return true;
}