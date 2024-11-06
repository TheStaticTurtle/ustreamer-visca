#pragma once

#include "viscaserver.h"

void us_viscaserver_init_udp_socket(us_viscaserver_s *viscaserver);

bool us_viscaserver_send_reply_buffer(us_viscaserver_s *viscaserver);


bool us_viscaserver_wait_for_packet(us_viscaserver_s *viscaserver);