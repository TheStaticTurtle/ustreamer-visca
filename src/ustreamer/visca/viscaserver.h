#pragma once

#include <sys/stat.h>

#include <stdatomic.h>


#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>


#include "../../libs/types.h"
#include "../../libs/list.h"
#include "../../libs/fpsi.h"

#include "../stream.h"

#include "./protocol.h"

#define VISCASERVER_BUFFER_SIZE 128

typedef struct {
	struct sockaddr_storage peeraddr;
	socklen_t peeraddr_len;

    char buffer[VISCASERVER_BUFFER_SIZE];
	int buffer_len;

    char reply_buffer[VISCASERVER_BUFFER_SIZE];
	int reply_buffer_len;

} us_viscaserver_packet;

typedef struct {
	struct sockaddr_in sin;
	int socket_fd;

	us_viscaserver_packet packet;

	atomic_bool			stop;

} us_viscaserver_runtime_s;

typedef struct us_viscasserver_sx {
	us_stream_s	*stream;

	us_viscaserver_runtime_s* run;

	char	*host;
	uint	port;

	char camera_address;

} us_viscaserver_s;


us_viscaserver_s *us_viscaserver_init(us_stream_s *stream);
void us_viscaserver_destroy(us_viscaserver_s *server);

void us_viscaserver_loop(us_viscaserver_s *server);
void us_viscaserver_loop_break(us_viscaserver_s *server);
