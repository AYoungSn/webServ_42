#include "../includes/Client.hpp"

Client::Client(Server *server)
{
	this->server = server;
	address_length = sizeof(struct sockaddr_storage);
	received_size = 0;
	gettimeofday(&last_get_time, NULL);
	memset(request, 0, MAX_REQUEST_SIZE + 1);
}

Client::~Client()
{
}

int Client::get_socket() const
{
	return socket;
}

int Client::get_received_size() const
{
	return received_size;
}

timeval Client::get_last_time() const
{
	return last_get_time;
};

void Client::set_last_time_sec(timeval& tv)
{
	last_get_time = tv;	
};

void Client::set_socket(int value)
{
	socket = value;

	fcntl(socket, F_SETFL, O_NONBLOCK);
	if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (void*)&server->recv_timeout, sizeof(struct timeval)) < 0)
		fprintf(stderr, "setsockopt: recv_timeout set failed\n");
	if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (void*)&server->send_timeout, sizeof(struct timeval)) < 0)
		fprintf(stderr, "setsockopt: send_timeout set failed\n");
}

void Client::set_received_size(int size)
{
	received_size = size;
}

const char *Client::get_client_address()
{
	static char address_buffer[100];
	getnameinfo((struct sockaddr *)&address, address_length,
		address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
	return address_buffer;
}

const char *Client::get_client_port()
{
	static char service_buffer[100];
	getnameinfo((struct sockaddr *)&address, address_length,
		0, 0, service_buffer, sizeof(service_buffer), NI_NUMERICHOST);
	return service_buffer;
}

std::string Client::get_root_path(std::string path)
{
	std::string root;
	int root_length = 0;
	int index = -1;

	for (int i = 0; i < server->locations.size(); i++)
	{
		if ((server->locations[i].root != "") && (path.find(server->locations[i].path) != std::string::npos))
		{
			if (count_char(path, '/') == count_char(server->locations[i].path, '/'))
			{
				if (strcmp(strrchr(path.c_str(), '/'), strrchr(server->locations[i].path.c_str(), '/')))
					continue;
			}
			if (root_length < server->locations[i].root.length())
			{
				root_length = server->locations[i].root.length();
				root = server->locations[i].root;
				index = i;
			}
		}
	}
	if (index == -1)
		root = server->root;
	return root;
}

std::string Client::get_autoindex_root_path(std::string path)
{
	std::string root = get_root_path("/");
	if (path == "/")
		return root;
	else
	{
		root = root + "/" + path;
		return root;
	}
}

int Client::count_char(std::string str, char c)
{
	int cnt = 0;

	for (int i = 0; i < str.size(); i++)
	{
		if (str[i] == c)
			cnt += 1;
	}
	return cnt;
} 
