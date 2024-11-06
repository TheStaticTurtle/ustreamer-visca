#include "viscaserver.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "../../libs/tools.h"
#include "../stream.h"

#include "protocol.h"
#include "tools.h"
#include "net.h"



us_viscaserver_s *us_viscaserver_init(us_stream_s *stream) {
	us_viscaserver_s *viscaserver;
	US_CALLOC(viscaserver, 1);
	viscaserver->stream = stream;

	viscaserver->host = "127.0.0.1";
	viscaserver->port = 1259;

	us_viscaserver_runtime_s *run;
	US_CALLOC(run, 1);

	atomic_init(&run->stop, false);

	// socket address structure specific to IPv4
	memset(&run->sin, 0, sizeof(run->sin));
	run->sin.sin_family = AF_INET;
	run->sin.sin_addr.s_addr = htonl(INADDR_ANY);; //inet_addr(viscaserver->host); 
	run->sin.sin_port = htons(viscaserver->port);

	viscaserver->run = run;

	return viscaserver;
}
void us_viscaserver_destroy(us_viscaserver_s *viscaserver) {
	free(viscaserver->run);
	free(viscaserver);
}

void us_viscaserver_loop(us_viscaserver_s *viscaserver) {
	us_viscaserver_runtime_s *const run = viscaserver->run;

	us_viscaserver_init_udp_socket(viscaserver);
	
	_LOG_INFO("Running ...");

	while (!atomic_load(&run->stop)) {
		us_viscaserver_wait_for_packet(viscaserver);
	}

	_LOG_INFO("Finished");
}
void us_viscaserver_loop_break(us_viscaserver_s *viscaserver) {
	atomic_store(&viscaserver->run->stop, true);
}

