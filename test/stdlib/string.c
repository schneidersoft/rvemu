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
    if (string != NULL) {
        strtok_position = string;
    }
    if (strtok_position == NULL) {
        return NULL;
    }

    /* Skip the delimiters that separate the previous token from this one. */
    while (*strtok_position != '\0') {
        const char *candidate = delimiters;
        int delimiter = 0;
        for (; *candidate != '\0'; candidate++) {
            if (*strtok_position == *candidate) {
                delimiter = 1;
                break;
            }
        }
        if (!delimiter) {
            break;
        }
        strtok_position++;
    }
    if (*strtok_position == '\0') {
        return NULL;
    }

    char *token = strtok_position;
    while (*strtok_position != '\0') {
        const char *candidate = delimiters;
        for (; *candidate != '\0'; candidate++) {
            if (*strtok_position == *candidate) {
                *strtok_position = '\0';
                strtok_position++;
                return token;
            }
        }
        strtok_position++;
    }
    return token;
}
