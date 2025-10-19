#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int __attribute__((used)) _kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}

int __attribute__((used)) _getpid(void) {
    return 1;
}

void _exit(int status) {
    while (1) {
        // Hang the system
    }
}

int _write(int file, char *ptr, int len) {
    return len;  // Assume the write is successful
}

int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _read(int file, char *ptr, int len) {
    return 0;  // No input available
}

int _open(const char *name, int flags, int mode) {
    return -1;
}

int _wait(int *status) {
    errno = ECHILD;
    return -1;
}

int _unlink(const char *name) {
    errno = ENOENT;
    return -1;
}

int _stat(const char *file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _link(const char *old, const char *new) {
    errno = EMLINK;
    return -1;
}

int _fork(void) {
    errno = EAGAIN;
    return -1;
}

int _execve(const char *name, char * const *argv, char * const *env) {
    errno = ENOMEM;
    return -1;
}