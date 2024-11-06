#pragma once

#include "viscaserver.h"


bool us_viscaserver_handle_commands(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_ptzoptics_commands(us_viscaserver_s *viscaserver);
bool us_viscaserver_handle_inqueries(us_viscaserver_s *viscaserver);
