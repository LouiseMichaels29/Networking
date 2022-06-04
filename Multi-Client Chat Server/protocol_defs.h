/*
    Helper Macros that add semantics to message codes
*/
#include <sys/socket.h>
#include "proj.h"

#define CODE_SERVER_FULL 0
#define CODE_SERVER_NOT_FULL 1
#define CODE_NEW_USER_CONNECT 2
#define CODE_USER_LEFT 3
#define CODE_USER_NEGO_MESS 4
#define CODE_MESS_TOO_LONG 5
#define CODE_RECIP_NOT_ACTIVE 6
#define CODE_BAD_MSG_FORMAT 7

#define UNC_USERNAME_TIME_EXP 0
#define UNC_USERNAME_TAKEN 1
#define UNC_USERNAME_TOO_LONG 2
#define UNC_USERNAME_INVALID 3
#define UNC_USERNAME_ACCEPT 4

#define MT_CONTROL 0
#define MT_CHAT 1

#define PUB_PRIV 0
#define PUB_PUB 1

#define PRV_NOT_PRIV 0
#define PRV_PRV 1

#define FRG_NOT_FRAG 0
#define FRG_FRG 1

#define LST_MORE_FRGS 0
#define LST_LAST_FRG 1

