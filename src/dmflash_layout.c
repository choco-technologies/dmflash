#include "dmflash_internal.h"
#include <errno.h>

static const char* skip_blanks(const char* p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

static int digit_of(char c, unsigned base)
{
    int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    return (d >= 0 && (unsigned)d < base) ? d : -1;
}

/* A number with an optional k / m suffix, from *p; *p moves past it */
static int take_number(const char** p, uint32_t* value)
{
    const char* s = skip_blanks(*p);
    unsigned base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    const char* digits = s;
    uint64_t n = 0;
    for (int d; (d = digit_of(*s, base)) >= 0; s++)
    {
        n = n * base + (unsigned)d;
        if (n > UINT32_MAX)
            return -ERANGE;
    }
    if (s == digits)
        return -EINVAL;
    if (base == 10 && (*s == 'k' || *s == 'K' || *s == 'm' || *s == 'M'))
    {
        n <<= (*s == 'k' || *s == 'K') ? 10 : 20;
        s++;
        if (n > UINT32_MAX)
            return -ERANGE;
    }
    *value = (uint32_t)n;
    *p = s;
    return 0;
}

int dmflash_parse_number(const char* text, uint32_t* value)
{
    if (text == NULL || value == NULL)
        return -EINVAL;
    const char* p = text;
    int rc = take_number(&p, value);
    return (rc == 0 && *skip_blanks(p) != '\0') ? -EINVAL : rc;
}

/* The next "<count>x<size>" (or just "<size>") of a layout: 1 - an item, 0 - the end, <0 - bad */
static int next_item(const char** p, uint32_t* count, uint32_t* size)
{
    const char* s = skip_blanks(*p);
    if (*s == '\0')
        return 0;
    uint32_t first;
    int rc = take_number(&s, &first);
    if (rc != 0)
        return rc;
    s = skip_blanks(s);
    *count = 1;
    *size = first;
    if (*s == 'x' || *s == 'X' || *s == '*')
    {
        *count = first;
        s++;
        rc = take_number(&s, size);
        if (rc != 0)
            return rc;
        s = skip_blanks(s);
    }
    if (*s != '\0' && *s != ',')
        return -EINVAL;
    *p = (*s == ',') ? s + 1 : s;
    return (*count == 0 || *size == 0) ? -EINVAL : 1;
}

/* How many sectors the layout holds (and that it is not absurd) */
static int count_sectors(const char* text, uint32_t* total)
{
    uint32_t sum = 0, count, size;
    const char* p = text;
    int rc;
    while ((rc = next_item(&p, &count, &size)) > 0)
    {
        if (count > DMFLASH_MAX_SECTORS || sum + count > DMFLASH_MAX_SECTORS)
            return -ERANGE;
        sum += count;
    }
    *total = sum;
    return (rc < 0) ? rc : (sum == 0 ? -EINVAL : 0);
}

int dmflash_layout_parse(const char* text, dmflash_sector_t** table, uint32_t* count)
{
    if (text == NULL || table == NULL || count == NULL)
        return -EINVAL;
    uint32_t total;
    int rc = count_sectors(text, &total);
    if (rc != 0)
        return rc;
    dmflash_sector_t* sectors = Dmod_Malloc(total * sizeof(*sectors));
    if (sectors == NULL)
        return -ENOMEM;
    uint64_t offset = 0;
    uint32_t index = 0, n, size;
    for (const char* p = text; next_item(&p, &n, &size) > 0; )
    {
        for (; n > 0; n--, index++)
        {
            sectors[index].offset = (uint32_t)offset;
            sectors[index].size = size;
            offset += size;
        }
    }
    if (offset > UINT32_MAX)
    {
        Dmod_Free(sectors);
        return -ERANGE;
    }
    *table = sectors;
    *count = total;
    return 0;
}

int dmflash_layout_find(const dmflash_sector_t* table, uint32_t count, uint32_t offset)
{
    for (uint32_t i = 0; table != NULL && i < count; i++)
    {
        if (table[i].offset == offset)
            return (int)i;
    }
    return -ENOENT;
}
