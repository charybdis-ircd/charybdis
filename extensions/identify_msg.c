#include <stdinc.h>
#include <modules.h>
#include <msgbuf.h>
#include <s_user.h>

static const char identify_msg_desc[] = "Provides the freenode.net/identify-msg client capability";

static void identmsg_outbound(void *);
unsigned int CLICAP_IDENTIFY_MSG = 0;

mapi_cap_list_av2 identmsg_cap_list[] = {
	{ MAPI_CAP_CLIENT, "freenode.net/identify-msg", NULL, &CLICAP_IDENTIFY_MSG },
	{ 0, NULL, NULL, NULL }
};

static mapi_hfn_list_av1 identmsg_hfnlist[] = {
	{ "outbound_msgbuf", identmsg_outbound },
	{ NULL, NULL }
};

static void identmsg_outbound(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (data->client->umodes & user_modes['r'])
		msgbuf_append_tag(msgbuf, "freenode.net/identified", NULL, CLICAP_IDENTIFY_MSG);
}

DECLARE_MODULE_AV2(identify_msg, NULL, NULL, NULL, NULL, identmsg_hfnlist, identmsg_cap_list, NULL, identify_msg_desc);
