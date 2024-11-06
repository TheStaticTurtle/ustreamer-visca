#include "net.h"
#include "tools.h"

#include "protocol.h"

#include "replies/errors.h"
#include "handle.h"

void us_viscaserver_init_udp_socket(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;

	run->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (run->socket_fd == -1) {
		_LOG_ERROR("socket failed")
		exit(1);
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	if (setsockopt(run->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv)) < 0) {
		_LOG_ERROR("Failed to set socket timeout");
		exit(1);
	}

	int optval = 1;
	if (setsockopt(run->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0) {
		_LOG_ERROR("Failed to set socket reuse");
		exit(1);
	}
		   
    if (bind(run->socket_fd, (struct sockaddr *)&run->sin, sizeof(run->sin)) != 0)  { 
		_LOG_ERROR("Bind failed")
		exit(1);
	}

	_LOG_INFO("Successfuly bound to %s:%d/udp", viscaserver->host, viscaserver->port);
}

bool us_viscaserver_send_reply_buffer(us_viscaserver_s *viscaserver) { 
	us_viscaserver_runtime_s *const run = viscaserver->run;

	return sendto( 
		run->socket_fd, 
		vs_res_pkt_buf, 
		vs_res_pkt_buf_len,  
		0,
		(struct sockaddr *)&run->packet.peeraddr,
		run->packet.peeraddr_len
	) > 0; 
}


bool us_viscaserver_wait_for_packet(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	memset(&run->packet, 0, sizeof(run->packet));

	run->packet.peeraddr_len = sizeof(struct sockaddr_storage);
    vs_req_pkt_buf_len = recvfrom(run->socket_fd, vs_req_pkt_buf, VISCASERVER_BUFFER_SIZE, 0, (struct sockaddr *) &run->packet.peeraddr, &run->packet.peeraddr_len);
    if (vs_req_pkt_buf_len < 1) { return false; }

	fprintf(stderr, "Recevied data len=%d data=", vs_req_pkt_buf_len);
	for (int i = 0; i < vs_req_pkt_buf_len; ++i) fprintf(stderr, "%02x ", vs_req_pkt_buf[i]);
	fprintf(stderr, "\n");

	VISCASERVER_PACKET_ASSERT_MINSIZE(2)

	VISCASERVER_PACKET_ASSERT_ADDRESS()

	VISCASERVER_PACKET_ASSERT_TERMINATOR()

	if(vs_req_pkt_buf[1] == VISCA_COMMAND) 					{ return us_viscaserver_handle_commands(viscaserver); }
	else if(vs_req_pkt_buf[1] == VISCA_COMMAND_PTZOPTICS)	{ return us_viscaserver_handle_ptzoptics_commands(viscaserver); }
	else if(vs_req_pkt_buf[1] == VISCA_INQUIRY)				{ return us_viscaserver_handle_inqueries(viscaserver); }
	else { return false; }
}