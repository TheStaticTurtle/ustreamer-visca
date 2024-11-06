#pragma once

#include "../viscaserver.h"


bool us_viscaserver_reply_error(us_viscaserver_s *viscaserver, char error_code);
bool us_viscaserver_reply_error_syntax(us_viscaserver_s *viscaserver);
bool us_viscaserver_reply_error_bufferfull(us_viscaserver_s *viscaserver) ;
bool us_viscaserver_reply_error_canceled(us_viscaserver_s *viscaserver);
bool us_viscaserver_reply_error_nosock(us_viscaserver_s *viscaserver) ;
bool us_viscaserver_reply_error_notexecutable(us_viscaserver_s *viscaserver);

bool us_viscaserver_reply_ack(us_viscaserver_s *viscaserver); 
bool us_viscaserver_reply_completion(us_viscaserver_s *viscaserver);