#include "TtyHandler.h"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>


int CTtyHandler::openTty(const char* pcTty, int iFlags)
{

    int iFd = open(pcTty, iFlags);

    if (iFd < 0) {

        return -1;
    }
    struct termios iosconfig;

    // Get config
    if(tcgetattr(iFd, &iosconfig) < 0) {

        close(iFd);

        return -1;
    }

    // Turn off input processing
    iosconfig.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                        INLCR | PARMRK | INPCK | ISTRIP | IXON);
    // Turn off output processing
    iosconfig.c_oflag = 0;
    // Turn off line processing
    iosconfig.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    // Turn off character processing
    iosconfig.c_cflag &= ~(CSIZE | PARENB);
    iosconfig.c_cflag |= CS8;
    // Min amount of bytes for a read
    iosconfig.c_cc[VMIN]  = 1;
    // No timeout
    iosconfig.c_cc[VTIME] = 0;

    // Set config
    if(tcsetattr(iFd, TCSAFLUSH, &iosconfig) < 0) {

        close(iFd);

        return -1;
    }
    return iFd;
}
