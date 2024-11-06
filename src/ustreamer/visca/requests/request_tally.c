#include "requests.h"
#include "../tools.h"
#include "../protocol.h"
#include "../replies/errors.h"


bool us_viscaserver_handle_command_tallylight_set(us_viscaserver_s *viscaserver) { 
	VISCASERVER_PACKET_ASSERT_SIZE(6)
	VISCASERVER_PACKET_ASSERT(syntax, ONEOF3(vs_req_pkt_buf[4], VISCA_VALUE_TALLY_OFF, VISCA_VALUE_TALLY_SOLID, VISCA_VALUE_TALLY_FLASHING))

	bool tally_on = vs_req_pkt_buf[4] != VISCA_VALUE_TALLY_OFF;
	bool tally_blink = vs_req_pkt_buf[4] == VISCA_VALUE_TALLY_FLASHING;

	_LOG_VERBOSE("Tally light setting tally_on=%d tally_blink=%d", tally_on, tally_blink);

	//TODO GPIO;

	return true;
}

