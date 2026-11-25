#include <stdlib.h>
#include <string.h>

void *memcpy(void *destination, const void *source, size_t length) {
    unsigned char *to = destination;
    const unsigned char *from = source;
    while (length--) {
        to[length] = from[length];
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t length) {
    unsigned char *to = destination;
    const unsigned char *from = source;

    if (to < from) {
        size_t i = 0;
        while (i < length) {
            to[i] = from[i];
            i++;
        }
    } else if (to > from) {
        size_t i = length;
        while (i > 0) {
            i--;
            to[i] = from[i];
        }
    }
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    unsigned char *to = destination;
    while (length--) {
        to[length] = (unsigned char)value;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t length) {
    const unsigned char *a = left;
    const unsigned char *b = right;
    while (length--) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
        a++;
        b++;
    }
    return 0;
}

size_t strlen(const char *string) {
    size_t length = 0;
    while (string[length] != '\0') {
        length++;
    }
    return length;
}

char *strcpy(char *destination, const char *source) {
    size_t i = 0;
    while ((destination[i] = source[i]) != '\0') {
        i++;
    }
    return destination;
}

char *strncpy(char *destination, const char *source, size_t length) {
    size_t i = 0;
    while (i < length && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }
    while (i < length) {
        destination[i++] = '\0';
    }
    return destination;
}

char *strcat(char *destination, const char *source) { return strcpy(destination + strlen(destination), source); }

char *strncat(char *destination, const char *source, size_t length) {
    size_t i = strlen(destination);
    size_t j = 0;
    while (j < length && source[j] != '\0') {
        destination[i++] = source[j++];
    }
    destination[i] = '\0';
    return destination;
}

int strcmp(const char *left, const char *right) {
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    if ((unsigned char)*left < (unsigned char)*right) {
        return -1;
    }
    if ((unsigned char)*left > (unsigned char)*right) {
        return 1;
    }
    return 0;
}

int strncmp(const char *left, const char *right, size_t length) {
    while (length > 0 && *left != '\0' && *left == *right) {
        left++;
        right++;
        length--;
    }
    if (length == 0) {
        return 0;
    }
    if ((unsigned char)*left < (unsigned char)*right) {
        return -1;
    }
    if ((unsigned char)*left > (unsigned char)*right) {
        return 1;
    }
    return 0;
}

char *strchr(const char *string, int character) {
    for (;;) {
        if (*string == (char)character) {
            return (char *)string;
        }
        if (*string == '\0') {
            return NULL;
        }
        string++;
    }
}

char *strrchr(const char *string, int character) {
    const char *last = NULL;
    for (;;) {
        if (*string == (char)character) {
            last = string;
        }
        if (*string == '\0') {
            return (char *)last;
        }
        string++;
    }
}

char *strstr(const char *haystack, const char *needle) {
    size_t length = strlen(needle);
    if (length == 0) {
        return (char *)haystack;
    }
    for (; *haystack != '\0'; haystack++) {
        if (*haystack == *needle && memcmp(haystack, needle, length) == 0) {
            return (char *)haystack;
        }
    }
    return NULL;
}

char *strpbrk(const char *string, const char *accept) {
    for (; *string != '\0'; string++) {
        const char *candidate = accept;
        for (; *candidate != '\0'; candidate++) {
            if (*string == *candidate) {
                return (char *)string;
            }
        }
    }
    return NULL;
}

size_t strspn(const char *string, const char *accept) {
    size_t length = 0;
    while (string[length] != '\0') {
        const char *candidate = accept;
        int found = 0;
        for (; *candidate != '\0'; candidate++) {
            if (string[length] == *candidate) {
                found = 1;
                break;
            }
        }
        if (!found) {
            break;
        }
        length++;
    }
    return length;
}

size_t strcspn(const char *string, const char *reject) {
    size_t length = 0;
    while (string[length] != '\0') {
        const char *candidate = reject;
        for (; *candidate != '\0'; candidate++) {
            if (string[length] == *candidate) {
                return length;
            }
        }
        length++;
    }
    return length;
}

static char *strtok_position;

char *strtok(char *string, const char *delimiters) {
    return strtok_r(string, delimiters, &strtok_position);
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (unsigned char)c) {
            return (void *)(p + i);
        }
    }
    return NULL;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') {
        n++;
    }
    return n;
}

char *strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *p = (char *)malloc(len + 1);
    if (p != NULL) {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *p = str != NULL ? str : *saveptr;
    if (p == NULL) {
        return NULL;
    }
    while (*p != '\0') {
        const char *d = delim;
        int is_delim = 0;
        for (; *d != '\0'; d++) {
            if (*p == *d) { is_delim = 1; break; }
        }
        if (!is_delim) break;
        p++;
    }
    if (*p == '\0') { *saveptr = p; return NULL; }
    char *token = p;
    while (*p != '\0') {
        const char *d = delim;
        for (; *d != '\0'; d++) {
            if (*p == *d) {
                *p = '\0';
                *saveptr = p + 1;
                return token;
            }
        }
        p++;
    }
    *saveptr = p;
    return token;
}

static int ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && ascii_tolower((unsigned char)*s1) == ascii_tolower((unsigned char)*s2)) {
        s1++; s2++;
    }
    return ascii_tolower((unsigned char)*s1) - ascii_tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int c1 = ascii_tolower((unsigned char)s1[i]);
        int c2 = ascii_tolower((unsigned char)s2[i]);
        if (c1 != c2) return c1 - c2;
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char *stpcpy(char *dest, const char *src) {
    while ((*dest = *src) != '\0') { dest++; src++; }
    return dest;
}

char *stpncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    char *end = dest + i;
    for (; i < n; i++) dest[i] = '\0';
    return end;
}
