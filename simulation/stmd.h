/* stmd_protocol.h
**
** Copyright (C) Intel 2011
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** Author: Guillaume Lucas <guillaumex.lucas@intel.com>
*/

#ifndef _STMD_H_
#define _STMD_H_

/* Sockets name */
#define SOCKET_NAME_MODEM_STATUS    "modem-status"
#define SOCKET_NAME_CLEAN_UP        "clean-up"

/* RRIL <-> STMD protocol */
enum ModemStatus {
    MODEM_DOWN,
    MODEM_UP,
    MODEM_COLD_RESET
};

#define MODEM_COLD_RESET_ACK    0x40

#endif // _STMD_H_

