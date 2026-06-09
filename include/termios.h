#ifndef LIBC_TERMIOS_H
#define LIBC_TERMIOS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define VINTR 0
#define VEOF 1
#define VERASE 2
#define VKILL 3
#define VMIN 4
#define VTIME 5
#define VEOL 6

#define BRKINT 0000002
#define ICRNL 0000400
#define IXON 0002000

#define OPOST 0000001
#define ONLCR 0000004

#define CS8 0000060
#define CREAD 0000200

#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020

#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);

#ifdef __cplusplus
}
#endif

#endif
