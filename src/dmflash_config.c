#include "dmflash_internal.h"
#include <string.h>

#define DEFAULT_SECTION "flash"

static const char* config_section(dmini_context_t ini)
{
    if (dmini_has_key(ini, DEFAULT_SECTION, "layout"))
        return DEFAULT_SECTION;
    if (dmini_has_key(ini, "dmflash", "layout"))
        return "dmflash";
    return dmini_section_count(ini) ? dmini_section_name(ini, 0) : DEFAULT_SECTION;
}

/* A number key (decimal, 0x hex, k / m): left as it is when the key is not there. false: not a number */
static bool read_number(dmini_context_t ini, const char* section, const char* key, uint32_t* value)
{
    const char* text = dmini_get_string(ini, section, key, NULL);
    return text == NULL || dmflash_parse_number(text, value) == 0;
}

static bool read_mode(dmini_context_t ini, const char* section, bool* read_only)
{
    const char* mode = dmini_get_string(ini, section, "mode", "ro");
    if (strcmp(mode, "ro") == 0)
        *read_only = true;
    else if (strcmp(mode, "rw") == 0)
        *read_only = false;
    else
        return false;
    return true;
}

bool dmflash_config_read(dmini_context_t ini, dmflash_config_t* config, const char** section)
{
    if (ini == NULL || config == NULL || section == NULL)
        return false;
    const char* s = config_section(ini);
    const char* base = dmini_get_string(ini, s, "base", NULL);
    bool ok = (base == NULL || dmflash_parse_address(base, &config->base) == 0) &&
              read_number(ini, s, "offset", &config->offset) &&
              read_number(ini, s, "size", &config->size) && read_number(ini, s, "program_bits", &config->program_bits) &&
              read_number(ini, s, "timeout_ms", &config->timeout_ms) && read_mode(ini, s, &config->read_only);
    config->layout = dmini_get_string(ini, s, "layout", NULL);
    *section = s;
    return ok && config->layout != NULL;
}
