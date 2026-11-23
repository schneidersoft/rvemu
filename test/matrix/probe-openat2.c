/* Probe 2: discriminate "guest flag bits misread by host ABI" (mips class)
 * from an order-dependent first-call artifact. Repeats the identical relative
 * call 3x, then the absolute path, then the per-arch native flag value. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void report(const char *label, int fd) {
    if (fd < 0) {
        printf("%-24s errno=%d (%s)\n", label, errno, strerror(errno));
    } else {
        printf("%-24s fd=%d OK\n", label, fd);
        close(fd);
    }
}

int main(void) {
    const char *rel = "probe-openat2.tmp";
    const char *abs = "/workspace/test/matrix/probe-openat2.tmp";
    char label[32];
    int i;

    printf("native O_RDWR|O_CREAT|O_TRUNC = 0x%x, AT_FDCWD = %d\n",
           O_RDWR | O_CREAT | O_TRUNC, (int)AT_FDCWD);

    printf("-- A: identical relative call, 3 attempts, guest flag bits 0x242 --\n");
    for (i = 1; i <= 3; i++) {
        unlink(rel);
        snprintf(label, sizeof(label), "A%u", i);
        report(label, openat((int)-100, rel, (int)0x242, 0644));
    }
    unlink(rel);

    printf("-- B: absolute path, guest flag bits 0x242 --\n");
    unlink(abs);
    report("B", openat((int)-100, abs, (int)0x242, 0644));
    unlink(abs);

    printf("-- C: relative, this-arch native flag value --\n");
    unlink(rel);
    report("C", openat((int)-100, rel, O_RDWR | O_CREAT | O_TRUNC, 0644));
    unlink(rel);

    return 0;
}
