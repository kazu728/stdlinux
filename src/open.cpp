#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int open(const char *path, int flags);
int open(const char *path, int flags, mode_t mode);