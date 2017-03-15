#include <stdio.h>
extern htlc_t *htlc_list;

static void cp_snd_linef (htlc_t *htlc, const char *fmt, ...);
static void cp_snd_numeric (htlc_t *htlc, const char numeric[3], const char *fmt, ...);

static struct protocol *irc_proto = 0;

static char * getHost (htlc_t *htlc);
//static char * satanize_nick (htlc_t *htlc);

static void irc_join (htlc_t *htlc, char *cmd, char *arg);




/* IRC PROTOCOL NUMERIC COMMANDS */
/* CONFORMS TO ftp://ftp.rfc-editor.org/in-notes/rfc2812.txt */
#define IRC_RPL_WELCOME		"001"
#define IRC_RPL_YOURHOST	"002"
#define IRC_RPL_CREATED		"003"
#define IRC_RPL_MYINFO		"004"
#define IRC_RPL_BOUNCE		"005"

#define IRC_RPL_USERHOST	"302"
#define IRC_RPL_ISON		"303"
#define IRC_RPL_AWAY		"301"
#define IRC_RPL_UNAWAY		"305"
#define IRC_RPL_NOWAWAY		"306"
#define IRC_RPL_WHOISUSER	"311"
#define IRC_RPL_WHOISSERVER	"312"
#define IRC_RPL_WHOISOPERATOR	"313"
#define IRC_RPL_WHOISIDLE	"317"
#define IRC_RPL_ENDOFWHOIS	"318"
#define IRC_RPL_WHOISCHANNELS	"319"
#define IRC_RPL_WHOASUSER	"314"
#define IRC_RPL_ENDOFWHOWAS	"369"
#define IRC_RPL_LIST		"322"
#define IRC_RPL_LISTEND		"323"
#define IRC_RPL_UNIQOPIS	"325"
#define IRC_RPL_CHANNELMODEIS	"324"
#define IRC_RPL_NOTOPIC		"331"
#define IRC_RPL_TOPIC		"332"
#define IRC_RPL_INVITING	"341"
#define IRC_RPL_SUMMONING	"342"
#define IRC_RPL_INVITELIST	"346"
#define IRC_RPL_ENDOFINVITELIST	"347"
#define IRC_RPL_EXCEPTLIST	"348"
#define IRC_RPL_ENDOFEXCEPTLIST	"349"
#define IRC_RPL_VERSION		"351"
#define IRC_RPL_WHOREPLY	"352"
#define IRC_RPL_ENDOFWHO	"315"
#define IRC_RPL_NAMREPLY	"353"
#define IRC_RPL_ENDOFNAMES	"366"
#define IRC_RPL_LINKS		"364"
#define IRC_RPL_ENDOFLINKS	"365"
#define IRC_RPL_BANLIST		"367"
#define IRC_RPL_ENDOFBANLIST	"368"
#define IRC_RPL_INFO		"371"
#define IRC_RPL_ENDOFINFO	"374"
#define IRC_RPL_MOTDSTART	"375"
#define IRC_RPL_MOTD		"372"
#define IRC_RPL_ENDOFMOTD	"376"
#define IRC_RPL_YOUREOPER	"381"
#define IRC_RPL_REHASHING	"382"
#define IRC_RPL_YOURESERVER	"383"
#define IRC_RPL_TIME		"391"
#define IRC_RPL_USERSTART	"392"
#define IRC_RPL_USERS		"393"
#define IRC_RPL_ENDOFUSERS	"394"
#define IRC_RPL_NOUSERS		"395"


/* Error Replies */
#define IRC_ERR_NOSUCHNICK	"401"
#define IRC_ERR_NOSUCHSERVER	"402"
#define IRC_ERR_NOSUCHCHANNEL	"403"
#define IRC_ERR_CANNOTSENDTOCHAN "404"
#define IRC_ERR_TOOMANYCHANNELS	"405"
#define IRC_ERR_WASNOSUCHNICK	"406"
#define IRC_ERR_TOOMANYTARGETS	"407"
#define IRC_ERR_NOSUCHSERVICE	"408"
#define IRC_ERR_NOORIGIN	"409"
#define IRC_ERR_NORECIPIENT	"411"
#define IRC_ERR_NOTEXTTOSEND	"412"
#define IRC_ERR_NOTOPLEVEL	"413"
#define IRC_ERR_WILDTOPLEVEL	"414"
#define IRC_ERR_BADMASK		"415"
#define IRC_ERR_UNKNOWNCOMMAND	"421"
#define IRC_ERR_NOMOTD		"422"
#define IRC_ERR_NOADMIINFO	"423"
#define IRC_ERR_FILEERROR	"424"
#define IRC_ERR_NOnICKNAMEGIVEN	"431"
#define IRC_ERR_ERRONEUSNICKNAME "432"
#define IRC_ERR_NICKNAMEINUSE	"433"
#define IRC_ERR_NICKCOLLISION	"436"
#define IRC_ERR_UNAVAILRESOURCE	"437"
#define IRC_ERR_USERNOTINCHANNEL "441"
#define IRC_ERR_NOLOGIN		"444"

