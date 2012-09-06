/* cosckets.c
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
** Author: Patrick Benavoli
*/
#include "sockets.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int socket_local_client(const char *name, int namespaceId, int type)
{
    // Simulated implementation relies on named pipe
    char acDeviceName[256];

    (void)namespaceId;
    (void)type;

    // Build pipe name
    strcpy(acDeviceName, "/tmp/");
    strcat(acDeviceName, name);

    return open(acDeviceName, O_RDWR|O_NONBLOCK);
}
