/* Probe: raw openat() with the exact numeric values the guest passes
 * (dirfd = -100, path = relative, flags = 0242 octal = O_RDWR|O_CREAT|O_TRUNC,
 * mode = 0644), plus the native-constant equivalent. Runs as a plain host
 * program (no emulator), per-arch under qemu-user, to bisect whether an
 * openat failure is an emulator defect or a qemu-user/glibc artifact. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void report(const char *label, int fd) {
    if (fd < 0) {
        printf("%-22s fd=-1 errno=%d (%s)\n", label, errno, strerror(errno));
    } else {
        printf("%-22s fd=%d OK\n", label, fd);
        close(fd);
    }
}

int main(void) {
    const char *path = "probe-openat.tmp";

    printf("arch native: O_RDWR|O_CREAT|O_TRUNC = %#x, AT_FDCWD = %d\n", O_RDWR | O_CREAT | O_TRUNC, AT_FDCWD);

    unlink(path);

    /* Exact guest numeric values. */
    report("guest-value openat", openat((int)-100, path, (int)0242, 0644));

    unlink(path);
    /* Native constants with identical semantics. */
    report("native openat", openat(AT_FDCWD, path, O_RDWR | O_CREAT | O_TRUNC, 0644));

    /* Was the file actually created? */
    report("post-stat open", open(path, O_RDONLY));
    unlink(path);
    return 0;
}
