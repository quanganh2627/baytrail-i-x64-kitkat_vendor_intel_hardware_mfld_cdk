#pragma once

class CTtyHandler {
public:
    static int openTty(const char* pcTty, int iFlags);
    static int setNonBlockingMode(const int fd_handler);
};

