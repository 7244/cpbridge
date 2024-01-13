#define INVALID_CODEPOINT 0xFFFD

#define GENERIC_SURROGATE_VALUE 0xD800
#define GENERIC_SURROGATE_MASK 0xF800

#define HIGH_SURROGATE_VALUE 0xD800
#define LOW_SURROGATE_VALUE 0xDC00
#define SURROGATE_MASK 0xFC00

#define SURROGATE_CODEPOINT_OFFSET 0x10000
#define SURROGATE_CODEPOINT_MASK 0x03FF
#define SURROGATE_CODEPOINT_BITS 10

// The highest codepoint that can be encoded with 1 byte in UTF-8
#define UTF8_1_MAX 0x7F
// The highest codepoint that can be encoded with 2 bytes in UTF-8
#define UTF8_2_MAX 0x7FF
// The highest codepoint that can be encoded with 3 bytes in UTF-8
#define UTF8_3_MAX 0xFFFF

#define UTF8_CONTINUATION_VALUE 0x80
#define UTF8_CONTINUATION_MASK 0xC0
#define UTF8_CONTINUATION_CODEPOINT_BITS 6

typedef uint32_t codepoint_t;

typedef struct
{
    // The mask that should be applied to the character before testing it
    uint8_t mask;
    // The value that the character should be tested against after applying the mask
    uint8_t value;
} utf8_pattern;

static const utf8_pattern utf8_leading_bytes[] =
{
    { 0x80, 0x00 }, // 0xxxxxxx
    { 0xE0, 0xC0 }, // 110xxxxx
    { 0xF0, 0xE0 }, // 1110xxxx
    { 0xF8, 0xF0 }  // 11110xxx
};

static codepoint_t decode_utf16(uint16_t const* utf16, size_t len, size_t* index)
{
    uint16_t high = utf16[*index];

    // BMP character
    if ((high & GENERIC_SURROGATE_MASK) != GENERIC_SURROGATE_VALUE)
        return high; 

    // Unmatched low surrogate, invalid
    if ((high & SURROGATE_MASK) != HIGH_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    // String ended with an unmatched high surrogate, invalid
    if (*index == len - 1)
        return INVALID_CODEPOINT;
    
    uint16_t low = utf16[*index + 1];

    // Unmatched high surrogate, invalid
    if ((low & SURROGATE_MASK) != LOW_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    // Two correctly matched surrogates, increase index to indicate we've consumed
    // two characters
    (*index)++;

    // The high bits of the codepoint are the value bits of the high surrogate
    // The low bits of the codepoint are the value bits of the low surrogate
    codepoint_t result = high & SURROGATE_CODEPOINT_MASK;
    result <<= SURROGATE_CODEPOINT_BITS;
    result |= low & SURROGATE_CODEPOINT_MASK;
    result += SURROGATE_CODEPOINT_OFFSET;
    
    // And if all else fails, it's valid
    return result;
}

static int calculate_utf8_len(codepoint_t codepoint)
{
    // An array with the max values would be more elegant, but a bit too heavy
    // for this common function

    if (codepoint <= UTF8_1_MAX)
        return 1;

    if (codepoint <= UTF8_2_MAX)
        return 2;

    if (codepoint <= UTF8_3_MAX)
        return 3;

    return 4;
}

static size_t encode_utf8(codepoint_t codepoint, uint8_t* utf8, size_t len, size_t index)
{
    int size = calculate_utf8_len(codepoint);

    // Not enough space left on the string
    if (index + size > len)
        return 0;

    // Write the continuation bytes in reverse order first
    for (int cont_index = size - 1; cont_index > 0; cont_index--)
    {
        uint8_t cont = codepoint & ~UTF8_CONTINUATION_MASK;
        cont |= UTF8_CONTINUATION_VALUE;

        utf8[index + cont_index] = cont;
        codepoint >>= UTF8_CONTINUATION_CODEPOINT_BITS;
    }

    // Write the leading byte
    utf8_pattern pattern = utf8_leading_bytes[size - 1];

    uint8_t lead = codepoint & ~(pattern.mask);
    lead |= pattern.value;

    utf8[index] = lead;

    return size;
}

size_t utf16_to_utf8(uint16_t const* utf16, size_t utf16_len, uint8_t* utf8, size_t utf8_len)
{
    // The next codepoint that will be written in the UTF-8 string
    // or the size of the required buffer if utf8 is NULL
    size_t utf8_index = 0;

    for (size_t utf16_index = 0; utf16_index < utf16_len; utf16_index++)
    {
        codepoint_t codepoint = decode_utf16(utf16, utf16_len, &utf16_index);

        if (utf8 == NULL)
            utf8_index += calculate_utf8_len(codepoint);
        else
            utf8_index += encode_utf8(codepoint, utf8, utf8_len, utf8_index);
    }

    return utf8_index;
}
