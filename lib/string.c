#include <lib/string.h>
#include <lib/stddef.h>

void *
memset(void *s, int c, size_t n)
{
    uint8_t *p;

    for (p = (uint8_t*)s; p < (uint8_t*)s + n; p++) {
        *p = c;
    }
    return s;
}

void*
memcpy(void *dest, const void *src, size_t n)
{
    const char *s;
    char *d;

    s = src;
    d = dest;

    while (n-- > 0) {
        *d++ = *s++;
    }

    return dest;
}

void*
memmove(void *dest, const void *src, size_t n)
{
    const char *s;
    char *d;

    s = src;
    d = dest;

    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0) {
            *--d = *--s;
        }
    } else {
        while (n-- > 0) {
            *d++ = *s++;
        }
    }

    return dest;
}

int
memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1, *p2;

    p1 = (const uint8_t*)s1;
    p2 = (const uint8_t*)s2;

    while (n-- > 0) {
        if (*p1 != *p2) {
            return (int)*p1 - (int)*p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

size_t
strlen(const char *s)
{
    size_t len;

    for (len = 0; *s != 0; s++, len++) { }
    return len;
}

char*
strcpy(char *dest, const char *src)
{
    char *d;

    d = dest;
    while ((*d++ = *src++) != 0) {
        ;
    }
    return dest;
}

char*
strncpy(char *dest, const char *src, size_t n)
{
    char *d;

    d = dest;
    while (n > 0 && (*d++ = *src++) != 0) {
        n--;
    }
    while (n > 0) {
        *d++ = 0, n--;
    }
    return dest;
}

int
strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++, s2++;
    }
    return *s1 - *s2;
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
    while (n > 0 && *s1 && *s1 == *s2) {
        n--, s1++, s2++;
    }
    if (n == 0) {
        return 0;
    }
    return *s1 - *s2;
}

/* Finds and returns the first occurrence of C in STRING, or a
   null pointer if C does not appear in STRING.  If C == '\0'
   then returns a pointer to the null terminator at the end of
   STRING. */
char*
strchr (const char *s, int c_)
{
    char c = c_;
    if (s == NULL) {
        return NULL;
    }

    for (;;) {
        if (*s == c) {
            return (char *) s;
        } else if (*s == '\0') {
            return NULL;
        }
        s++;
    }
}

char*
strtok_r (char *s, const char *delimiters, char **save_ptr)
{
    char *token;
    if (delimiters == NULL || save_ptr == NULL) {
        return NULL;
    }

    /* If S is nonnull, start from it.
        If S is null, start from saved position. */
    if (s == NULL) {
        s = *save_ptr;
    }

    /* Skip any DELIMITERS at our current position. */
    while (strchr (delimiters, *s) != NULL) {
        /* strchr() will always return nonnull if we're searching
            for a null byte, because every string contains a null
            byte (at the end). */
        if (*s == '\0') {
            *save_ptr = s;
            return NULL;
        }
        s++;
    }

    /* Skip any non-DELIMITERS up to the end of the string. */
    token = s;
    while (strchr (delimiters, *s) == NULL) {
        s++;
    }
    if (*s != '\0') {
        *s = '\0';
        *save_ptr = s + 1;
    } else {
        *save_ptr = s;
    }
    return token;
}
