
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-6-7.
//

#include "unicode.h"

#include <locale.h>

#include "clib.h"
#include "mirror-priv.h"
#include "unicode-comp-priv.h"
#include "unicode-chars-priv.h"
#include "unicode-script-priv.h"
#include "unicode-decomp-priv.h"


#define SBase 0xAC00
#define LBase 0x1100
#define VBase 0x1161
#define TBase 0x11A7
#define LCount 19
#define VCount 21
#define TCount 28
#define NCount (VCount * TCount)
#define SCount (LCount * NCount)

#define C_UNICHAR_FULLWIDTH_A 0xff21
#define C_UNICHAR_FULLWIDTH_I 0xff29
#define C_UNICHAR_FULLWIDTH_J 0xff2a
#define C_UNICHAR_FULLWIDTH_F 0xff26
#define C_UNICHAR_FULLWIDTH_a 0xff41
#define C_UNICHAR_FULLWIDTH_f 0xff46

#define C_SCRIPT_TABLE_MIDPOINT (C_N_ELEMENTS (gsScriptTable) / 2)
#define C_WIDTH_TABLE_MIDPOINT  (C_N_ELEMENTS (gsUnicodeWidthTableWide) / 2)

#define SURROGATE_VALUE(h,l)    (((h) - 0xd800) * 0x400 + (l) - 0xdc00 + 0x10000)

#define UTF8_COMPUTE(Char, Mask, Len)                                         \
    if (Char < 128) {                                                         \
        Len = 1;                                                              \
        Mask = 0x7f;                                                          \
    }                                                                         \
    else if ((Char & 0xe0) == 0xc0) {                                         \
        Len = 2;                                                              \
        Mask = 0x1f;                                                          \
    }                                                                         \
    else if ((Char & 0xf0) == 0xe0) {                                         \
        Len = 3;                                                              \
        Mask = 0x0f;                                                          \
    }                                                                         \
    else if ((Char & 0xf8) == 0xf0) {                                         \
        Len = 4;                                                              \
        Mask = 0x07;                                                          \
    }                                                                         \
    else if ((Char & 0xfc) == 0xf8) {                                         \
        Len = 5;                                                              \
        Mask = 0x03;                                                          \
    }                                                                         \
    else if ((Char & 0xfe) == 0xfc) {                                         \
        Len = 6;                                                              \
        Mask = 0x01;                                                          \
    }                                                                         \
    else {                                                                    \
        Len = -1;                                                             \
    }
#define UTF8_LENGTH(Char)               \
    ((Char) < 0x80 ? 1 :                \
    ((Char) < 0x800 ? 2 :               \
    ((Char) < 0x10000 ? 3 :             \
    ((Char) < 0x200000 ? 4 :            \
    ((Char) < 0x4000000 ? 5 : 6)))))

#define UTF8_GET(Result, Chars, Count, Mask, Len)                             \
    (Result) = (Chars)[0] & (Mask);                                           \
    for ((Count) = 1; (Count) < (Len); ++(Count)) {                           \
        if (((Chars)[(Count)] & 0xc0) != 0x80) {                              \
            (Result) = -1;                                                    \
            break;                                                            \
        }                                                                     \
        (Result) <<= 6;                                                       \
        (Result) |= ((Chars)[(Count)] & 0x3f);                                \
    }

#define UNICODE_VALID(Char)                   \
    ((Char) < 0x110000 &&                     \
    (((Char) & 0xFFFFF800) != 0xD800))

#define CONT_BYTE_FAST(p) ((cuchar)*p++ & 0x3f)
#define VALIDATE_BYTE(mask, expect)                             \
    C_STMT_START {                                              \
        if (C_UNLIKELY((*(cuchar *)p & (mask)) != (expect)))    \
            goto error;                                         \
    } C_STMT_END

#define CC_PART1(Page, Char) \
  ((gsCombiningClassTablePart1[Page] >= C_UNICODE_MAX_TABLE_INDEX) \
   ? (gsCombiningClassTablePart1[Page] - C_UNICODE_MAX_TABLE_INDEX) \
   : (gsClassData[gsCombiningClassTablePart1[Page]][Char]))

#define CC_PART2(Page, Char) \
  ((gsCombiningClassTablePart2[Page] >= C_UNICODE_MAX_TABLE_INDEX) \
   ? (gsCombiningClassTablePart2[Page] - C_UNICODE_MAX_TABLE_INDEX) \
   : (gsClassData[gsCombiningClassTablePart2[Page]][Char]))

#define COMBINING_CLASS(Char) \
  (((Char) <= C_UNICODE_LAST_CHAR_PART1) \
   ? CC_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= C_UNICODE_LAST_CHAR) \
      ? CC_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : 0))

#define CI(Page, Char) \
  ((gsComposeTable[Page] >= C_UNICODE_MAX_TABLE_INDEX) \
   ? (gsComposeTable[Page] - C_UNICODE_MAX_TABLE_INDEX) \
   : (gsComposeData[gsComposeTable[Page]][Char]))

#define COMPOSE_INDEX(Char) \
     (((Char >> 8) > (COMPOSE_TABLE_LAST)) ? 0 : CI((Char) >> 8, (Char) & 0xff))

#define ATTR_TABLE(Page) (((Page) <= C_UNICODE_LAST_PAGE_PART1) \
                          ? gsAttrTablePart1[Page] \
                          : gsAttrTablePart2[(Page) - 0xe00])

#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == C_UNICODE_MAX_TABLE_INDEX) ? 0 : (gsAttrData[ATTR_TABLE(Page)][Char]))

#define TTYPE_PART1(Page, Char) \
  ((gsTypeTablePart1[Page] >= C_UNICODE_MAX_TABLE_INDEX) \
   ? (gsTypeTablePart1[Page] - C_UNICODE_MAX_TABLE_INDEX) \
   : (gsTypeData[gsTypeTablePart1[Page]][Char]))

#define TTYPE_PART2(Page, Char) \
  ((gsTypeTablePart2[Page] >= C_UNICODE_MAX_TABLE_INDEX) \
   ? (gsTypeTablePart2[Page] - C_UNICODE_MAX_TABLE_INDEX) \
   : (gsTypeData[gsTypeTablePart2[Page]][Char]))

#define TYPE(Char) \
  (((Char) <= C_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= C_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : C_UNICODE_UNASSIGNED))

#define IS(Type, Class) (((cuint)1 << (Type)) & (Class))
#define OR(Type, Rest)  (((cuint)1 << (Type)) | (Rest))

#define UNICODE_ISALPHA(Type)   IS ((Type), \
                            OR (C_UNICODE_LOWERCASE_LETTER,     \
                            OR (C_UNICODE_UPPERCASE_LETTER,     \
                            OR (C_UNICODE_TITLECASE_LETTER,     \
                            OR (C_UNICODE_MODIFIER_LETTER,      \
                            OR (C_UNICODE_OTHER_LETTER,         0))))))

#define UNICODE_ISALDIGIT(Type) IS ((Type), \
                            OR (C_UNICODE_DECIMAL_NUMBER,       \
                            OR (C_UNICODE_LETTER_NUMBER,        \
                            OR (C_UNICODE_OTHER_NUMBER,         \
                            OR (C_UNICODE_LOWERCASE_LETTER,     \
                            OR (C_UNICODE_UPPERCASE_LETTER,     \
                            OR (C_UNICODE_TITLECASE_LETTER,     \
                            OR (C_UNICODE_MODIFIER_LETTER,      \
                            OR (C_UNICODE_OTHER_LETTER,         0)))))))))

#define UNICODE_ISMARK(Type)    IS ((Type), \
                            OR (C_UNICODE_NON_SPACING_MARK,     \
                            OR (C_UNICODE_SPACING_MARK, \
                            OR (C_UNICODE_ENCLOSING_MARK,       0))))

#define UNICODE_ISZEROWIDTHTYPE(Type)   IS ((Type),  \
                            OR (C_UNICODE_NON_SPACING_MARK,     \
                            OR (C_UNICODE_ENCLOSING_MARK,       \
                            OR (C_UNICODE_FORMAT,               0))))

typedef enum
{
    LOCALE_NORMAL,
    LOCALE_TURKIC,
    LOCALE_LITHUANIAN
} LocaleType;


static LocaleType get_locale_type (void);
static bool has_more_above (const cchar *str);
static const cchar* fast_validate (const char *str);
static inline bool c_unichar_iswide_bsearch (cunichar ch);
static int interval_compare (const void *key, const void *elt);
static bool combine (cunichar a, cunichar b, cunichar *result);
static const cchar* find_decomposition (cunichar ch, bool compat);
static bool combine_hangul (cunichar a, cunichar b, cunichar *result);
static const cchar* fast_validate_len (const char *str, cssize max_len);
static inline CUnicodeScript c_unichar_get_script_bsearch (cunichar ch);
static void decompose_hangul (cunichar s, cunichar *r, csize* result_len);
static bool decompose_hangul_step (cunichar ch, cunichar* a, cunichar* b);
static void* try_malloc_n (csize n_blocks, csize n_block_bytes, CError **error);
static inline cunichar c_utf8_get_char_extended (const cchar *p, cssize max_len);
static cint output_marks (const char **p_inout, char* out_buffer, bool remove_dot);
static csize real_tolower (const cchar *str, cssize max_len, cchar* out_buffer, LocaleType locale_type);
static csize real_toupper (const cchar *str, cssize max_len, cchar* out_buffer, LocaleType locale_type);


cunichar* _c_utf8_normalize_wc (const cchar* str, cssize maxLen, CNormalizeMode mode);

static const char gsUtf8SkipData[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};


/* http://unicode.org/iso15924/ */
static const cuint32 gsISO15924Tags[] =
    {
#define PACK(a,b,c,d) ((cuint32)((((cuint8)(a))<<24)|(((cuint8)(b))<<16)|(((cuint8)(c))<<8)|((cuint8)(d))))

        PACK ('Z','y','y','y'), /* G_UNICODE_SCRIPT_COMMON */
        PACK ('Z','i','n','h'), /* G_UNICODE_SCRIPT_INHERITED */
        PACK ('A','r','a','b'), /* G_UNICODE_SCRIPT_ARABIC */
        PACK ('A','r','m','n'), /* G_UNICODE_SCRIPT_ARMENIAN */
        PACK ('B','e','n','g'), /* G_UNICODE_SCRIPT_BENGALI */
        PACK ('B','o','p','o'), /* G_UNICODE_SCRIPT_BOPOMOFO */
        PACK ('C','h','e','r'), /* G_UNICODE_SCRIPT_CHEROKEE */
        PACK ('C','o','p','t'), /* G_UNICODE_SCRIPT_COPTIC */
        PACK ('C','y','r','l'), /* G_UNICODE_SCRIPT_CYRILLIC */
        PACK ('D','s','r','t'), /* G_UNICODE_SCRIPT_DESERET */
        PACK ('D','e','v','a'), /* G_UNICODE_SCRIPT_DEVANAGARI */
        PACK ('E','t','h','i'), /* G_UNICODE_SCRIPT_ETHIOPIC */
        PACK ('G','e','o','r'), /* G_UNICODE_SCRIPT_GEORGIAN */
        PACK ('G','o','t','h'), /* G_UNICODE_SCRIPT_GOTHIC */
        PACK ('G','r','e','k'), /* G_UNICODE_SCRIPT_GREEK */
        PACK ('G','u','j','r'), /* G_UNICODE_SCRIPT_GUJARATI */
        PACK ('G','u','r','u'), /* G_UNICODE_SCRIPT_GURMUKHI */
        PACK ('H','a','n','i'), /* G_UNICODE_SCRIPT_HAN */
        PACK ('H','a','n','g'), /* G_UNICODE_SCRIPT_HANGUL */
        PACK ('H','e','b','r'), /* G_UNICODE_SCRIPT_HEBREW */
        PACK ('H','i','r','a'), /* G_UNICODE_SCRIPT_HIRAGANA */
        PACK ('K','n','d','a'), /* G_UNICODE_SCRIPT_KANNADA */
        PACK ('K','a','n','a'), /* G_UNICODE_SCRIPT_KATAKANA */
        PACK ('K','h','m','r'), /* G_UNICODE_SCRIPT_KHMER */
        PACK ('L','a','o','o'), /* G_UNICODE_SCRIPT_LAO */
        PACK ('L','a','t','n'), /* G_UNICODE_SCRIPT_LATIN */
        PACK ('M','l','y','m'), /* G_UNICODE_SCRIPT_MALAYALAM */
        PACK ('M','o','n','g'), /* G_UNICODE_SCRIPT_MONGOLIAN */
        PACK ('M','y','m','r'), /* G_UNICODE_SCRIPT_MYANMAR */
        PACK ('O','g','a','m'), /* G_UNICODE_SCRIPT_OGHAM */
        PACK ('I','t','a','l'), /* G_UNICODE_SCRIPT_OLD_ITALIC */
        PACK ('O','r','y','a'), /* G_UNICODE_SCRIPT_ORIYA */
        PACK ('R','u','n','r'), /* G_UNICODE_SCRIPT_RUNIC */
        PACK ('S','i','n','h'), /* G_UNICODE_SCRIPT_SINHALA */
        PACK ('S','y','r','c'), /* G_UNICODE_SCRIPT_SYRIAC */
        PACK ('T','a','m','l'), /* G_UNICODE_SCRIPT_TAMIL */
        PACK ('T','e','l','u'), /* G_UNICODE_SCRIPT_TELUGU */
        PACK ('T','h','a','a'), /* G_UNICODE_SCRIPT_THAANA */
        PACK ('T','h','a','i'), /* G_UNICODE_SCRIPT_THAI */
        PACK ('T','i','b','t'), /* G_UNICODE_SCRIPT_TIBETAN */
        PACK ('C','a','n','s'), /* G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL */
        PACK ('Y','i','i','i'), /* G_UNICODE_SCRIPT_YI */
        PACK ('T','g','l','g'), /* G_UNICODE_SCRIPT_TAGALOG */
        PACK ('H','a','n','o'), /* G_UNICODE_SCRIPT_HANUNOO */
        PACK ('B','u','h','d'), /* G_UNICODE_SCRIPT_BUHID */
        PACK ('T','a','g','b'), /* G_UNICODE_SCRIPT_TAGBANWA */

        /* Unicode-4.0 additions */
        PACK ('B','r','a','i'), /* G_UNICODE_SCRIPT_BRAILLE */
        PACK ('C','p','r','t'), /* G_UNICODE_SCRIPT_CYPRIOT */
        PACK ('L','i','m','b'), /* G_UNICODE_SCRIPT_LIMBU */
        PACK ('O','s','m','a'), /* G_UNICODE_SCRIPT_OSMANYA */
        PACK ('S','h','a','w'), /* G_UNICODE_SCRIPT_SHAVIAN */
        PACK ('L','i','n','b'), /* G_UNICODE_SCRIPT_LINEAR_B */
        PACK ('T','a','l','e'), /* G_UNICODE_SCRIPT_TAI_LE */
        PACK ('U','g','a','r'), /* G_UNICODE_SCRIPT_UGARITIC */

        /* Unicode-4.1 additions */
        PACK ('T','a','l','u'), /* G_UNICODE_SCRIPT_NEW_TAI_LUE */
        PACK ('B','u','g','i'), /* G_UNICODE_SCRIPT_BUGINESE */
        PACK ('G','l','a','g'), /* G_UNICODE_SCRIPT_GLAGOLITIC */
        PACK ('T','f','n','g'), /* G_UNICODE_SCRIPT_TIFINAGH */
        PACK ('S','y','l','o'), /* G_UNICODE_SCRIPT_SYLOTI_NAGRI */
        PACK ('X','p','e','o'), /* G_UNICODE_SCRIPT_OLD_PERSIAN */
        PACK ('K','h','a','r'), /* G_UNICODE_SCRIPT_KHAROSHTHI */

        /* Unicode-5.0 additions */
        PACK ('Z','z','z','z'), /* G_UNICODE_SCRIPT_UNKNOWN */
        PACK ('B','a','l','i'), /* G_UNICODE_SCRIPT_BALINESE */
        PACK ('X','s','u','x'), /* G_UNICODE_SCRIPT_CUNEIFORM */
        PACK ('P','h','n','x'), /* G_UNICODE_SCRIPT_PHOENICIAN */
        PACK ('P','h','a','g'), /* G_UNICODE_SCRIPT_PHAGS_PA */
        PACK ('N','k','o','o'), /* G_UNICODE_SCRIPT_NKO */

        /* Unicode-5.1 additions */
        PACK ('K','a','l','i'), /* G_UNICODE_SCRIPT_KAYAH_LI */
        PACK ('L','e','p','c'), /* G_UNICODE_SCRIPT_LEPCHA */
        PACK ('R','j','n','g'), /* G_UNICODE_SCRIPT_REJANG */
        PACK ('S','u','n','d'), /* G_UNICODE_SCRIPT_SUNDANESE */
        PACK ('S','a','u','r'), /* G_UNICODE_SCRIPT_SAURASHTRA */
        PACK ('C','h','a','m'), /* G_UNICODE_SCRIPT_CHAM */
        PACK ('O','l','c','k'), /* G_UNICODE_SCRIPT_OL_CHIKI */
        PACK ('V','a','i','i'), /* G_UNICODE_SCRIPT_VAI */
        PACK ('C','a','r','i'), /* G_UNICODE_SCRIPT_CARIAN */
        PACK ('L','y','c','i'), /* G_UNICODE_SCRIPT_LYCIAN */
        PACK ('L','y','d','i'), /* G_UNICODE_SCRIPT_LYDIAN */

        /* Unicode-5.2 additions */
        PACK ('A','v','s','t'), /* G_UNICODE_SCRIPT_AVESTAN */
        PACK ('B','a','m','u'), /* G_UNICODE_SCRIPT_BAMUM */
        PACK ('E','g','y','p'), /* G_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS */
        PACK ('A','r','m','i'), /* G_UNICODE_SCRIPT_IMPERIAL_ARAMAIC */
        PACK ('P','h','l','i'), /* G_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI */
        PACK ('P','r','t','i'), /* G_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN */
        PACK ('J','a','v','a'), /* G_UNICODE_SCRIPT_JAVANESE */
        PACK ('K','t','h','i'), /* G_UNICODE_SCRIPT_KAITHI */
        PACK ('L','i','s','u'), /* G_UNICODE_SCRIPT_LISU */
        PACK ('M','t','e','i'), /* G_UNICODE_SCRIPT_MEETEI_MAYEK */
        PACK ('S','a','r','b'), /* G_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN */
        PACK ('O','r','k','h'), /* G_UNICODE_SCRIPT_OLD_TURKIC */
        PACK ('S','a','m','r'), /* G_UNICODE_SCRIPT_SAMARITAN */
        PACK ('L','a','n','a'), /* G_UNICODE_SCRIPT_TAI_THAM */
        PACK ('T','a','v','t'), /* G_UNICODE_SCRIPT_TAI_VIET */

        /* Unicode-6.0 additions */
        PACK ('B','a','t','k'), /* G_UNICODE_SCRIPT_BATAK */
        PACK ('B','r','a','h'), /* G_UNICODE_SCRIPT_BRAHMI */
        PACK ('M','a','n','d'), /* G_UNICODE_SCRIPT_MANDAIC */

        /* Unicode-6.1 additions */
        PACK ('C','a','k','m'), /* G_UNICODE_SCRIPT_CHAKMA */
        PACK ('M','e','r','c'), /* G_UNICODE_SCRIPT_MEROITIC_CURSIVE */
        PACK ('M','e','r','o'), /* G_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS */
        PACK ('P','l','r','d'), /* G_UNICODE_SCRIPT_MIAO */
        PACK ('S','h','r','d'), /* G_UNICODE_SCRIPT_SHARADA */
        PACK ('S','o','r','a'), /* G_UNICODE_SCRIPT_SORA_SOMPENG */
        PACK ('T','a','k','r'), /* G_UNICODE_SCRIPT_TAKRI */

        /* Unicode 7.0 additions */
        PACK ('B','a','s','s'), /* G_UNICODE_SCRIPT_BASSA_VAH */
        PACK ('A','g','h','b'), /* G_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN */
        PACK ('D','u','p','l'), /* G_UNICODE_SCRIPT_DUPLOYAN */
        PACK ('E','l','b','a'), /* G_UNICODE_SCRIPT_ELBASAN */
        PACK ('G','r','a','n'), /* G_UNICODE_SCRIPT_GRANTHA */
        PACK ('K','h','o','j'), /* G_UNICODE_SCRIPT_KHOJKI*/
        PACK ('S','i','n','d'), /* G_UNICODE_SCRIPT_KHUDAWADI */
        PACK ('L','i','n','a'), /* G_UNICODE_SCRIPT_LINEAR_A */
        PACK ('M','a','h','j'), /* G_UNICODE_SCRIPT_MAHAJANI */
        PACK ('M','a','n','i'), /* G_UNICODE_SCRIPT_MANICHAEAN */
        PACK ('M','e','n','d'), /* G_UNICODE_SCRIPT_MENDE_KIKAKUI */
        PACK ('M','o','d','i'), /* G_UNICODE_SCRIPT_MODI */
        PACK ('M','r','o','o'), /* G_UNICODE_SCRIPT_MRO */
        PACK ('N','b','a','t'), /* G_UNICODE_SCRIPT_NABATAEAN */
        PACK ('N','a','r','b'), /* G_UNICODE_SCRIPT_OLD_NORTH_ARABIAN */
        PACK ('P','e','r','m'), /* G_UNICODE_SCRIPT_OLD_PERMIC */
        PACK ('H','m','n','g'), /* G_UNICODE_SCRIPT_PAHAWH_HMONG */
        PACK ('P','a','l','m'), /* G_UNICODE_SCRIPT_PALMYRENE */
        PACK ('P','a','u','c'), /* G_UNICODE_SCRIPT_PAU_CIN_HAU */
        PACK ('P','h','l','p'), /* G_UNICODE_SCRIPT_PSALTER_PAHLAVI */
        PACK ('S','i','d','d'), /* G_UNICODE_SCRIPT_SIDDHAM */
        PACK ('T','i','r','h'), /* G_UNICODE_SCRIPT_TIRHUTA */
        PACK ('W','a','r','a'), /* G_UNICODE_SCRIPT_WARANG_CITI */

        /* Unicode 8.0 additions */
        PACK ('A','h','o','m'), /* G_UNICODE_SCRIPT_AHOM */
        PACK ('H','l','u','w'), /* G_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS */
        PACK ('H','a','t','r'), /* G_UNICODE_SCRIPT_HATRAN */
        PACK ('M','u','l','t'), /* G_UNICODE_SCRIPT_MULTANI */
        PACK ('H','u','n','g'), /* G_UNICODE_SCRIPT_OLD_HUNGARIAN */
        PACK ('S','g','n','w'), /* G_UNICODE_SCRIPT_SIGNWRITING */

        /* Unicode 9.0 additions */
        PACK ('A','d','l','m'), /* G_UNICODE_SCRIPT_ADLAM */
        PACK ('B','h','k','s'), /* G_UNICODE_SCRIPT_BHAIKSUKI */
        PACK ('M','a','r','c'), /* G_UNICODE_SCRIPT_MARCHEN */
        PACK ('N','e','w','a'), /* G_UNICODE_SCRIPT_NEWA */
        PACK ('O','s','g','e'), /* G_UNICODE_SCRIPT_OSAGE */
        PACK ('T','a','n','g'), /* G_UNICODE_SCRIPT_TANGUT */

        /* Unicode 10.0 additions */
        PACK ('G','o','n','m'), /* G_UNICODE_SCRIPT_MASARAM_GONDI */
        PACK ('N','s','h','u'), /* G_UNICODE_SCRIPT_NUSHU */
        PACK ('S','o','y','o'), /* G_UNICODE_SCRIPT_SOYOMBO */
        PACK ('Z','a','n','b'), /* G_UNICODE_SCRIPT_ZANABAZAR_SQUARE */

        /* Unicode 11.0 additions */
        PACK ('D','o','g','r'), /* G_UNICODE_SCRIPT_DOGRA */
        PACK ('G','o','n','g'), /* G_UNICODE_SCRIPT_GUNJALA_GONDI */
        PACK ('R','o','h','g'), /* G_UNICODE_SCRIPT_HANIFI_ROHINGYA */
        PACK ('M','a','k','a'), /* G_UNICODE_SCRIPT_MAKASAR */
        PACK ('M','e','d','f'), /* G_UNICODE_SCRIPT_MEDEFAIDRIN */
        PACK ('S','o','g','o'), /* G_UNICODE_SCRIPT_OLD_SOGDIAN */
        PACK ('S','o','g','d'), /* G_UNICODE_SCRIPT_SOGDIAN */

        /* Unicode 12.0 additions */
        PACK ('E','l','y','m'), /* G_UNICODE_SCRIPT_ELYMAIC */
        PACK ('N','a','n','d'), /* G_UNICODE_SCRIPT_NANDINAGARI */
        PACK ('H','m','n','p'), /* G_UNICODE_SCRIPT_NYIAKENG_PUACHUE_HMONG */
        PACK ('W','c','h','o'), /* G_UNICODE_SCRIPT_WANCHO */

        /* Unicode 13.0 additions */
        PACK ('C', 'h', 'r', 's'), /* G_UNICODE_SCRIPT_CHORASMIAN */
        PACK ('D', 'i', 'a', 'k'), /* G_UNICODE_SCRIPT_DIVES_AKURU */
        PACK ('K', 'i', 't', 's'), /* G_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT */
        PACK ('Y', 'e', 'z', 'i'), /* G_UNICODE_SCRIPT_YEZIDI */

        /* Unicode 14.0 additions */
        PACK ('C', 'p', 'm', 'n'), /* G_UNICODE_SCRIPT_CYPRO_MINOAN */
        PACK ('O', 'u', 'g', 'r'), /* G_UNICODE_SCRIPT_OLD_UYHUR */
        PACK ('T', 'n', 's', 'a'), /* G_UNICODE_SCRIPT_TANGSA */
        PACK ('T', 'o', 't', 'o'), /* G_UNICODE_SCRIPT_TOTO */
        PACK ('V', 'i', 't', 'h'), /* G_UNICODE_SCRIPT_VITHKUQI */

        /* not really a Unicode script, but part of ISO 15924 */
        PACK ('Z', 'm', 't', 'h'), /* G_UNICODE_SCRIPT_MATH */

        /* Unicode 15.0 additions */
        PACK ('K', 'a', 'w', 'i'), /* G_UNICODE_SCRIPT_KAWI */
        PACK ('N', 'a', 'g', 'm'), /* G_UNICODE_SCRIPT_NAG_MUNDARI */

#undef PACK
    };

static const struct Interval gsDefaultWideBlocks[] = {
    { 0x3400, 0x4dbf },
    { 0x4e00, 0x9fff },
    { 0xf900, 0xfaff },
    { 0x20000, 0x2fffd },
    { 0x30000, 0x3fffd }
};

const char * const gsUtf8Skip = gsUtf8SkipData;

char *c_utf8_find_prev_char(const char *str, const char *p)
{
    while (p > str) {
        --p;
        if ((*p & 0xc0) != 0x80) {
            return (char*)p;
        }
    }

    return NULL;
}

char *c_utf8_find_next_char(const char *p, const char *end)
{
    if (end) {
        for (++p; p < end && (*p & 0xc0) == 0x80; ++p)
            ;
        return (p >= end) ? NULL : (char*)p;
    }
    else {
        for (++p; (*p & 0xc0) == 0x80; ++p)
            ;
        return (char*)p;
    }
}

char *c_utf8_prev_char(const char *p)
{
    while (true) {
        p--;
        if ((*p & 0xc0) != 0x80) {
            return (char*) p;
        }
    }

    return NULL;
}

clong c_utf8_strlen(const char *p, cssize max)
{
    clong len = 0;
    const char *start = p;
    c_return_val_if_fail (p != NULL || max == 0, 0);

    if (max < 0) {
        while (*p) {
            p = c_utf8_next_char (p);
            ++len;
        }
    }
    else {
        if (max == 0 || !*p)
            return 0;

        p = c_utf8_next_char (p);

        while (p - start < max && *p) {
            ++len;
            p = c_utf8_next_char (p);
        }

        /* only do the last len increment if we got a complete
         * char (don't count partial chars)
         */
        if (p - start <= max) {
            ++len;
        }
    }

    return len;
}

char *c_utf8_substring(const char *str, clong start_pos, clong end_pos)
{
    char *start, *end, *out;

    c_return_val_if_fail (end_pos >= start_pos || end_pos == -1, NULL);

    start = c_utf8_offset_to_pointer (str, start_pos);

    if (end_pos == -1) {
        long length = c_utf8_strlen (start, -1);
        end = c_utf8_offset_to_pointer (start, length);
    }
    else {
        end = c_utf8_offset_to_pointer (start, end_pos - start_pos);
    }

    out = c_malloc0 (end - start + 1);
    memcpy (out, start, end - start);
    out[end - start] = 0;

    return out;
}

cunichar c_utf8_get_char(const cchar *p)
{
    int i, mask = 0, len;
    cunichar result;
    unsigned char c = (unsigned char) *p;

    UTF8_COMPUTE (c, mask, len);
    if (len == -1) {
        return (cunichar)-1;
    }
    UTF8_GET (result, p, i, mask, len);

    return result;
}

cchar *c_utf8_offset_to_pointer(const cchar *str, clong offset)
{
    const cchar *s = str;

    if (offset > 0) {
        while (offset--) {
            s = c_utf8_next_char (s);
        }
    }
    else {
        const char *s1;
        while (offset) {
            s1 = s;
            s += offset;
            while ((*s & 0xc0) == 0x80) {
                s--;
            }
            offset += c_utf8_pointer_to_offset (s, s1);
        }
    }

    return (cchar*)s;
}

clong c_utf8_pointer_to_offset(const cchar *str, const cchar *pos)
{
    const cchar *s = str;
    clong offset = 0;

    if (pos < str) {
        offset = - c_utf8_pointer_to_offset (pos, str);
    }
    else {
        while (s < pos) {
            s = c_utf8_next_char (s);
            offset++;
        }
    }

    return offset;
}

cchar *c_utf8_strncpy(cchar *dest, const cchar *src, csize n)
{
    const cchar *s = src;
    while (n && *s) {
        s = c_utf8_next_char(s);
        n--;
    }
    strncpy(dest, src, s - src);
    dest[s - src] = 0;
    return dest;
}

cint c_unichar_to_utf8(cunichar c, cchar *outbuf)
{
    cuint len = 0;
    int first;
    int i;

    if (c < 0x80) {
        first = 0;
        len = 1;
    }
    else if (c < 0x800) {
        first = 0xc0;
        len = 2;
    }
    else if (c < 0x10000) {
        first = 0xe0;
        len = 3;
    }
    else if (c < 0x200000) {
        first = 0xf0;
        len = 4;
    }
    else if (c < 0x4000000) {
        first = 0xf8;
        len = 5;
    }
    else {
        first = 0xfc;
        len = 6;
    }

    if (outbuf) {
        for (i = len - 1; i > 0; --i) {
            outbuf[i] = (c & 0x3f) | 0x80;
            c >>= 6;
        }
        outbuf[0] = c | first;
    }

    return len;
}

cchar *c_utf8_strchr(const char *p, cssize len, cunichar c)
{
    cchar ch[10];

    cint charlen = c_unichar_to_utf8 (c, ch);
    ch[charlen] = '\0';

    return c_strstr_len (p, len, ch);
}

cchar *c_utf8_strrchr(const char *p, cssize len, cunichar c)
{
    cchar ch[10];

    cint charlen = c_unichar_to_utf8 (c, ch);
    ch[charlen] = '\0';

    return c_strrstr_len (p, len, ch);
}

cunichar c_utf8_get_char_validated(const cchar *p, cssize maxLen)
{
    cunichar result;

    if (maxLen == 0) {
        return (cunichar)-2;
    }

    result = c_utf8_get_char_extended (p, maxLen);

    if (result == 0 && maxLen > 0) {
        return (cunichar) -2;
    }

    if (result & 0x80000000) {
        return result;
    }
    else if (!UNICODE_VALID (result)) {
        return (cunichar)-1;
    }
    else {
        return result;
    }

    return 0;
}

cunichar *c_utf8_to_ucs4_fast(const cchar *str, clong len, clong *itemsWritten)
{
    cunichar *result;
    cint n_chars, i;
    const cchar *p;

    c_return_val_if_fail (str != NULL, NULL);

    p = str;
    n_chars = 0;
    if (len < 0) {
        while (*p) {
            p = c_utf8_next_char (p);
            ++n_chars;
        }
    }
    else {
        while (p < str + len && *p) {
            p = c_utf8_next_char (p);
            ++n_chars;
        }
    }

    result = c_malloc0(sizeof(cunichar) * (n_chars + 1));

    p = str;
    for (i = 0; i < n_chars; i++) {
        cuchar first = (cuchar)*p++;
        cunichar wc;
        if (first < 0xc0) {
            wc = first;
        }
        else {
            cunichar c1 = CONT_BYTE_FAST(p);
            if (first < 0xe0) {
                wc = ((first & 0x1f) << 6) | c1;
            }
            else {
                cunichar c2 = CONT_BYTE_FAST(p);
                if (first < 0xf0) {
                    wc = ((first & 0x0f) << 12) | (c1 << 6) | c2;
                }
                else {
                    cunichar c3 = CONT_BYTE_FAST(p);
                    wc = ((first & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
                    if (C_UNLIKELY (first >= 0xf8)) {
                        cunichar mask = 1 << 20;
                        while ((wc & mask) != 0) {
                            wc <<= 6;
                            wc |= CONT_BYTE_FAST(p);
                            mask <<= 5;
                        }
                        wc &= mask - 1;
                    }
                }
            }
        }
        result[i] = wc;
    }
    result[i] = 0;

    if (itemsWritten) {
        *itemsWritten = i;
    }

    return result;
}

cunichar *c_utf8_to_ucs4(const cchar *str, clong len, clong *itemsRead, clong *itemsWritten, CError **error)
{
    cunichar *result = NULL;
    cint n_chars, i;
    const cchar *in;

    in = str;
    n_chars = 0;
    while ((len < 0 || str + len - in > 0) && *in) {
        cunichar wc = c_utf8_get_char_extended (in, len < 0 ? 6 : str + len - in);
        if (wc & 0x80000000) {
            if (wc == (cunichar)-2) {
                if (itemsRead) {
                    break;
                }
                else {
                    c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_PARTIAL_INPUT,
                                            _("Partial character sequence at end of input"));
                }
            }
            else {
                c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                        _("Invalid byte sequence in conversion input"));
            }

            goto err_out;
        }

        n_chars++;

        in = c_utf8_next_char (in);
    }

    result = try_malloc_n (n_chars + 1, sizeof (cunichar), error);
    if (result == NULL) {
        goto err_out;
    }

    in = str;
    for (i=0; i < n_chars; i++) {
        result[i] = c_utf8_get_char (in);
        in = c_utf8_next_char (in);
    }
    result[i] = 0;

    if (itemsWritten) {
        *itemsWritten = n_chars;
    }

err_out:
    if (itemsRead) {
        *itemsRead = in - str;
    }

    return result;
}

cchar *c_ucs4_to_utf8(const cunichar *str, clong len, clong *itemsRead, clong *itemsWritten, CError **error)
{
    cint result_length;
    cchar *result = NULL;
    cchar *p;
    cint i;

    result_length = 0;
    for (i = 0; len < 0 || i < len ; i++) {
        if (!str[i])
            break;

        if (str[i] >= 0x80000000) {
            c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                    _("Character out of range for UTF-8"));
            goto err_out;
        }

        result_length += UTF8_LENGTH (str[i]);
    }

    result = try_malloc_n (result_length + 1, 1, error);
    if (result == NULL) {
        goto err_out;
    }

    p = result;

    i = 0;
    while (p < result + result_length) {
        p += c_unichar_to_utf8 (str[i++], p);
    }

    *p = '\0';

    if (itemsWritten) {
        *itemsWritten = p - result;
    }

err_out:
    if (itemsRead) {
        *itemsRead = i;
    }

    return result;
}

cchar *c_utf16_to_utf8(const cunichar2 *str, clong len, clong *itemsRead, clong *itemsWritten, CError **error)
{
    const cunichar2 *in;
    cchar *out;
    cchar *result = NULL;
    cint n_bytes;
    cunichar high_surrogate;

    c_return_val_if_fail (str != NULL, NULL);

    n_bytes = 0;
    in = str;
    high_surrogate = 0;
    while ((len < 0 || in - str < len) && *in) {
        cunichar2 c = *in;
        cunichar wc;

        if (c >= 0xdc00 && c < 0xe000) {
            if (high_surrogate) {
                wc = SURROGATE_VALUE (high_surrogate, c);
                high_surrogate = 0;
            }
            else {
                c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                        _("Invalid sequence in conversion input"));
                goto err_out;
            }
        }
        else {
            if (high_surrogate) {
                c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                        _("Invalid sequence in conversion input"));
                goto err_out;
            }

            if (c >= 0xd800 && c < 0xdc00) {
                high_surrogate = c;
                goto next1;
            }
            else {
                wc = c;
            }
        }

        /********** DIFFERENT for UTF8/UCS4 **********/
        n_bytes += UTF8_LENGTH (wc);

next1:
        in++;
    }

    if (high_surrogate && !itemsRead) {
        c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_PARTIAL_INPUT,
                                _("Partial character sequence at end of input"));
        goto err_out;
    }

    /********** DIFFERENT for UTF8/UCS4 **********/
    result = try_malloc_n (n_bytes + 1, 1, error);
    if (result == NULL) {
        goto err_out;
    }

    high_surrogate = 0;
    out = result;
    in = str;
    while (out < result + n_bytes) {
        cunichar2 c = *in;
        cunichar wc;

        if (c >= 0xdc00 && c < 0xe000) {
            wc = SURROGATE_VALUE (high_surrogate, c);
            high_surrogate = 0;
        }
        else if (c >= 0xd800 && c < 0xdc00) {
            /* high surrogate */
            high_surrogate = c;
            goto next2;
        }
        else
            wc = c;

        /********** DIFFERENT for UTF8/UCS4 **********/
        out += c_unichar_to_utf8 (wc, out);
next2:
        in++;
    }

    /********** DIFFERENT for UTF8/UCS4 **********/
    *out = '\0';

    if (itemsWritten) {
        /********** DIFFERENT for UTF8/UCS4 **********/
        *itemsWritten = out - result;
    }

err_out:
    if (itemsRead) {
        *itemsRead = in - str;
    }

    return result;
}

cunichar *c_utf16_to_ucs4(const cunichar2 *str, clong len, clong *itemsRead, clong *itemsWritten, CError **error)
{
    const cunichar2 *in;
    cchar *out;
    cchar *result = NULL;
    cint n_bytes;
    cunichar high_surrogate;

    c_return_val_if_fail (str != NULL, NULL);

    n_bytes = 0;
    in = str;
    high_surrogate = 0;
    while ((len < 0 || in - str < len) && *in) {
        cunichar2 c = *in;

        if (c >= 0xdc00 && c < 0xe000) {
            /* low surrogate */
            if (high_surrogate) {
                high_surrogate = 0;
            }
            else {
                c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                        _("Invalid sequence in conversion input"));
                goto err_out;
            }
        }
        else {
            if (high_surrogate) {
                c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                        _("Invalid sequence in conversion input"));
                goto err_out;
            }

            if (c >= 0xd800 && c < 0xdc00) {
                /* high surrogate */
                high_surrogate = c;
                goto next1;
            }
        }

        /********** DIFFERENT for UTF8/UCS4 **********/
        n_bytes += sizeof (cunichar);

next1:
        in++;
    }

    if (high_surrogate && !itemsRead) {
        c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_PARTIAL_INPUT,
                                _("Partial character sequence at end of input"));
        goto err_out;
    }

    /********** DIFFERENT for UTF8/UCS4 **********/
    result = try_malloc_n (n_bytes + 4, 1, error);
    if (result == NULL) {
        goto err_out;
    }

    high_surrogate = 0;
    out = result;
    in = str;
    while (out < result + n_bytes) {
        cunichar2 c = *in;
        cunichar wc;

        if (c >= 0xdc00 && c < 0xe000) {
            /* low surrogate */
            wc = SURROGATE_VALUE (high_surrogate, c);
            high_surrogate = 0;
        }
        else if (c >= 0xd800 && c < 0xdc00) {
            /* high surrogate */
            high_surrogate = c;
            goto next2;
        }
        else {
            wc = c;
        }

        /********** DIFFERENT for UTF8/UCS4 **********/
        *(cunichar*)out = wc;
        out += sizeof (cunichar);

next2:
        in++;
    }

    /********** DIFFERENT for UTF8/UCS4 **********/
    *(cunichar*)out = 0;

    if (itemsWritten) {
        /********** DIFFERENT for UTF8/UCS4 **********/
        *itemsWritten = (out - result) / sizeof (cunichar);
    }

err_out:
    if (itemsRead) {
        *itemsRead = in - str;
    }

    return (cunichar*) result;
}

cunichar2 *c_utf8_to_utf16(const cchar *str, clong len, clong *itemsRead, clong *itemsWritten, CError **error)
{
    cunichar2 *result = NULL;
    cint n16;
    const cchar *in;
    cint i;

    c_return_val_if_fail (str != NULL, NULL);

    in = str;
    n16 = 0;
    while ((len < 0 || str + len - in > 0) && *in) {
        cunichar wc = c_utf8_get_char_extended (in, len < 0 ? 6 : str + len - in);
        if (wc & 0x80000000) {
            if (wc == (cunichar)-2) {
                if (itemsRead) {
                    break;
                }
                else {
                    c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_PARTIAL_INPUT,
                                            _("Partial character sequence at end of input"));
                }
            }
            else {
                c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                        _("Invalid byte sequence in conversion input"));
            }
            goto err_out;
        }

        if (wc < 0xd800) {
            n16 += 1;
        }
        else if (wc < 0xe000) {
            c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                    _("Invalid sequence in conversion input"));
            goto err_out;
        }
        else if (wc < 0x10000) {
            n16 += 1;
        }
        else if (wc < 0x110000) {
            n16 += 2;
        }
        else {
            c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                    _("Character out of range for UTF-16"));
            goto err_out;
        }
        in = c_utf8_next_char (in);
    }

    result = try_malloc_n (n16 + 1, sizeof (cunichar2), error);
    if (result == NULL) {
        goto err_out;
    }

    in = str;
    for (i = 0; i < n16;) {
        cunichar wc = c_utf8_get_char (in);
        if (wc < 0x10000) {
            result[i++] = wc;
        }
        else {
            result[i++] = (wc - 0x10000) / 0x400 + 0xd800;
            result[i++] = (wc - 0x10000) % 0x400 + 0xdc00;
        }
        in = c_utf8_next_char (in);
    }

    result[i] = 0;

    if (itemsWritten) {
        *itemsWritten = n16;
    }

err_out:
    if (itemsRead) {
        *itemsRead = in - str;
    }

    return result;
}

cunichar2 *c_ucs4_to_utf16(const cunichar *str, clong len, clong *itemsRead, clong *itemsWritten, CError **error)
{
    cunichar2 *result = NULL;
    cint n16;
    cint i, j;

    n16 = 0;
    i = 0;
    while ((len < 0 || i < len) && str[i]) {
        cunichar wc = str[i];

        if (wc < 0xd800) {
            n16 += 1;
        }
        else if (wc < 0xe000) {
            c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                    _("Invalid sequence in conversion input"));
            goto err_out;
        }
        else if (wc < 0x10000) {
            n16 += 1;
        }
        else if (wc < 0x110000) {
            n16 += 2;
        }
        else {
            c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                    _("Character out of range for UTF-16"));
            goto err_out;
        }

        i++;
    }

    result = try_malloc_n (n16 + 1, sizeof (cunichar2), error);
    if (result == NULL) {
        goto err_out;
    }

    for (i = 0, j = 0; j < n16; i++) {
        cunichar wc = str[i];
        if (wc < 0x10000) {
            result[j++] = wc;
        }
        else {
            result[j++] = (wc - 0x10000) / 0x400 + 0xd800;
            result[j++] = (wc - 0x10000) % 0x400 + 0xdc00;
        }
    }
    result[j] = 0;

    if (itemsWritten) {
        *itemsWritten = n16;
    }

err_out:
    if (itemsRead) {
        *itemsRead = i;
    }

    return result;
}

bool c_utf8_validate(const char *str, cssize max_len, const cchar **end)
{
    const cchar *p;

    if (max_len >= 0) {
        return c_utf8_validate_len (str, max_len, end);
    }

    p = fast_validate (str);

    if (end) {
        *end = p;
    }

    if (*p != '\0') {
        return false;
    }
    else {
        return true;
    }
}

bool c_utf8_validate_len(const char *str, csize max_len, const cchar **end)
{
    const cchar *p;

    p = fast_validate_len (str, max_len);

    if (end) {
        *end = p;
    }

    if (p != str + max_len) {
        return false;
    }
    else {
        return true;
    }
}

bool c_unichar_validate(cunichar ch)
{
    return UNICODE_VALID (ch);
}

cchar *c_utf8_strreverse(const cchar *str, cssize len)
{
    cchar *r, *result;
    const cchar *p;

    if (len < 0) {
        len = strlen (str);
    }

    result = c_malloc0(sizeof(cchar) * (len + 1));
    r = result + len;
    p = str;
    while (r > result) {
        cchar *m, skip = gsUtf8Skip[*(cuchar*) p];
        r -= skip;
        c_assert (r >= result);
        for (m = r; skip; skip--) {
            *m++ = *p++;
        }
    }
    result[len] = 0;

    return result;
}

cchar *c_utf8_make_valid(const cchar *str, cssize len)
{
    CString *string;
    const cchar *remainder, *invalid;
    csize remaining_bytes, valid_bytes;

    c_return_val_if_fail (str != NULL, NULL);

    if (len < 0) {
        len = strlen (str);
    }

    string = NULL;
    remainder = str;
    remaining_bytes = len;

    while (remaining_bytes != 0) {
        if (c_utf8_validate (remainder, remaining_bytes, &invalid)) {
            break;
        }
        valid_bytes = invalid - remainder;

        if (string == NULL) {
            string = c_string_sized_new (remaining_bytes);
        }

        c_string_append_len (string, remainder, valid_bytes);
        /* append U+FFFD REPLACEMENT CHARACTER */
        c_string_append (string, "\357\277\275");

        remaining_bytes -= valid_bytes + 1;
        remainder = invalid + 1;
    }

    if (string == NULL)
        return c_strndup (str, len);

    c_string_append_len (string, remainder, remaining_bytes);
    c_string_append_c (string, '\0');

    c_assert (c_utf8_validate (string->str, -1, NULL));

    return c_string_free (string, false);
}

cint c_unichar_combining_class(cunichar c)
{
    return COMBINING_CLASS (c);
}

void c_unicode_canonical_ordering(cunichar *str, csize len)
{
    csize i;
    int swap = 1;

    while (swap) {
        int last;
        swap = 0;
        last = COMBINING_CLASS (str[0]);
        for (i = 0; i < len - 1; ++i) {
            int next = COMBINING_CLASS (str[i + 1]);
            if (next != 0 && last > next) {
                csize j;
                /* Percolate item leftward through string.  */
                for (j = i + 1; j > 0; --j) {
                    cunichar t;
                    if (COMBINING_CLASS (str[j - 1]) <= next) {
                        break;
                    }
                    t = str[j];
                    str[j] = str[j - 1];
                    str[j - 1] = t;
                    swap = 1;
                }
                next = last;
            }
            last = next;
        }
    }
}

cunichar *c_unicode_canonical_decomposition(cunichar ch, csize *result_len)
{
    const cchar *decomp;
    const cchar *p;
    cunichar *r;

    /* Hangul syllable */
    if (ch >= SBase && ch < SBase + SCount) {
        decompose_hangul (ch, NULL, result_len);
        r = c_malloc0 (*result_len * sizeof (cunichar));
        decompose_hangul (ch, r, result_len);
    }
    else if ((decomp = find_decomposition (ch, false)) != NULL) {
        /* Found it.  */
        int i;

        *result_len = c_utf8_strlen (decomp, -1);
        r = c_malloc0 (*result_len * sizeof (cunichar));

        for (p = decomp, i = 0; *p != '\0'; p = c_utf8_next_char (p), i++) {
            r[i] = c_utf8_get_char (p);
        }
    }
    else {
        /* Not in our table.  */
        r = c_malloc0 (sizeof (cunichar));
        *r = ch;
        *result_len = 1;
    }

    return r;
}

cchar *c_utf8_normalize(const cchar *str, cssize len, CNormalizeMode mode)
{
    cunichar *result_wc = _c_utf8_normalize_wc (str, len, mode);
    cchar *result;

    result = c_ucs4_to_utf8 (result_wc, -1, NULL, NULL, NULL);
    c_free (result_wc);

    return result;
}

bool c_unichar_decompose(cunichar ch, cunichar *a, cunichar *b)
{
    cint start = 0;
    cint end = C_N_ELEMENTS (gsDecompStepTable);

    if (decompose_hangul_step (ch, a, b))
        return true;

    /* TODO use bsearch() */
    if (ch >= gsDecompStepTable[start].ch &&
        ch <= gsDecompStepTable[end - 1].ch)
    {
        while (true) {
            cint half = (start + end) / 2;
            const DecompositionStep *p = &(gsDecompStepTable[half]);
            if (ch == p->ch) {
                *a = p->a;
                *b = p->b;
                return true;
            }
            else if (half == start) {
                break;
            }
            else if (ch > p->ch) {
                start = half;
            }
            else {
                end = half;
            }
        }
    }

    *a = ch;
    *b = 0;

    return false;
}

bool c_unichar_compose(cunichar a, cunichar b, cunichar *ch)
{
    if (combine (a, b, ch)) {
        return true;
    }

    *ch = 0;
    return false;
}

csize c_unichar_fully_decompose(cunichar ch, bool compat, cunichar *result, csize resultLen)
{
    const cchar *decomp;
    const cchar *p;

    /* Hangul syllable */
    if (ch >= SBase && ch < SBase + SCount) {
        csize len, i;
        cunichar buffer[3];
        decompose_hangul (ch, result ? buffer : NULL, &len);
        if (result) {
            for (i = 0; i < len && i < resultLen; i++) {
                result[i] = buffer[i];
            }
        }
        return len;
    }
    else if ((decomp = find_decomposition (ch, compat)) != NULL) {
        /* Found it.  */
        csize len, i;

        len = c_utf8_strlen (decomp, -1);
        for (p = decomp, i = 0; i < len && i < resultLen; p = c_utf8_next_char (p), i++) {
            result[i] = c_utf8_get_char (p);
        }

        return len;
    }

    /* Does not decompose */
    if (result && resultLen >= 1) {
        *result = ch;
    }

    return 1;
}

bool c_unichar_isalnum(cunichar c)
{
    return UNICODE_ISALDIGIT (TYPE (c)) ? true: false;
}

bool c_unichar_isalpha(cunichar c)
{
    return UNICODE_ISALPHA (TYPE (c)) ? true : false;
}

bool c_unichar_iscntrl(cunichar c)
{
    return TYPE (c) == C_UNICODE_CONTROL;
}

bool c_unichar_isdigit(cunichar c)
{
    return TYPE (c) == C_UNICODE_DECIMAL_NUMBER;
}

bool c_unichar_isgraph(cunichar c)
{
    return !IS (TYPE(c),
                OR (C_UNICODE_CONTROL,
                OR (C_UNICODE_FORMAT,
                OR (C_UNICODE_UNASSIGNED,
                OR (C_UNICODE_SURROGATE,
                OR (C_UNICODE_SPACE_SEPARATOR,
                0))))));
}

bool c_unichar_islower(cunichar c)
{
    return TYPE (c) == C_UNICODE_LOWERCASE_LETTER;
}
bool c_unichar_isprint(cunichar c)
{
    return !IS (TYPE(c),
                OR (C_UNICODE_CONTROL,
                OR (C_UNICODE_FORMAT,
                OR (C_UNICODE_UNASSIGNED,
                OR (C_UNICODE_SURROGATE,
                0)))));
}

bool c_unichar_ispunct(cunichar c)
{
    return IS (TYPE(c),
               OR (C_UNICODE_CONNECT_PUNCTUATION,
               OR (C_UNICODE_DASH_PUNCTUATION,
               OR (C_UNICODE_CLOSE_PUNCTUATION,
               OR (C_UNICODE_FINAL_PUNCTUATION,
               OR (C_UNICODE_INITIAL_PUNCTUATION,
               OR (C_UNICODE_OTHER_PUNCTUATION,
               OR (C_UNICODE_OPEN_PUNCTUATION,
               OR (C_UNICODE_CURRENCY_SYMBOL,
               OR (C_UNICODE_MODIFIER_SYMBOL,
               OR (C_UNICODE_MATH_SYMBOL,
               OR (C_UNICODE_OTHER_SYMBOL,
               0)))))))))))) ? true : false;
}

bool c_unichar_isspace(cunichar c)
{
    switch (c) {
        case '\t':
        case '\n':
        case '\r':
        case '\f': {
            return true;
            break;
        }
        default: {
            return IS (TYPE(c),
                       OR (C_UNICODE_SPACE_SEPARATOR,
                       OR (C_UNICODE_LINE_SEPARATOR,
                       OR (C_UNICODE_PARAGRAPH_SEPARATOR,
                       0)))) ? true : false;
            break;
        }
    }
}

bool c_unichar_ismark(cunichar c)
{
    return UNICODE_ISMARK (TYPE (c));
}

bool c_unichar_isupper(cunichar c)
{
    return TYPE (c) == C_UNICODE_UPPERCASE_LETTER;
}

bool c_unichar_istitle(cunichar c)
{
    unsigned int i;
    for (i = 0; i < C_N_ELEMENTS (gsTitleTable); ++i) {
        if (gsTitleTable[i][0] == c) {
            return true;
        }
    }

    return false;
}

bool c_unichar_isxdigit(cunichar c)
{
    return ((c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F')
        || (c >= C_UNICHAR_FULLWIDTH_a && c <= C_UNICHAR_FULLWIDTH_f)
        || (c >= C_UNICHAR_FULLWIDTH_A && c <= C_UNICHAR_FULLWIDTH_F)
        || (TYPE (c) == C_UNICODE_DECIMAL_NUMBER));
}

bool c_unichar_isdefined(cunichar c)
{
    return !IS (TYPE(c),
                OR (C_UNICODE_UNASSIGNED,
                OR (C_UNICODE_SURROGATE,
                0)));
}

bool c_unichar_iszerowidth(cunichar c)
{
    if (C_UNLIKELY (c == 0x00AD)) {
        return false;
    }

    if (C_UNLIKELY (UNICODE_ISZEROWIDTHTYPE (TYPE (c)))) {
        return true;
    }

    /* A few additional codepoints are zero-width:
     *  - Part of the Hangul Jamo block covering medial/vowels/jungseong and
     *    final/trailing_consonants/jongseong Jamo
     *  - Jungseong and jongseong for Old Korean
     *  - Zero-width space (U+200B)
     */
    if (C_UNLIKELY ((c >= 0x1160 && c < 0x1200)
            || (c >= 0xD7B0 && c < 0xD800)
            || c == 0x200B)) {
        return true;
    }

    return false;
}

bool c_unichar_iswide(cunichar c) {
    if (c < gsUnicodeWidthTableWide[0].start) {
        return false;
    } else if (c_unichar_iswide_bsearch(c)) {
        return true;
    } else if (c_unichar_type(c) == C_UNICODE_UNASSIGNED
               && bsearch(C_UINT_TO_POINTER (c),
                          gsDefaultWideBlocks,
                          C_N_ELEMENTS (gsDefaultWideBlocks),
                          sizeof gsDefaultWideBlocks[0],
                          interval_compare)) {
        return true;
    }

    return false;
}

bool c_unichar_iswide_cjk(cunichar c)
{
    if (c_unichar_iswide (c)) {
        return true;
    }

    if (c == 0) {
        return false;
    }

    if (bsearch (C_UINT_TO_POINTER (c),
                 gsUnicodeWidthTableAmbiguous,
                 C_N_ELEMENTS (gsUnicodeWidthTableAmbiguous),
                 sizeof gsUnicodeWidthTableAmbiguous[0],
                 interval_compare)) {
        return true;
    }

    return false;
}

cunichar c_unichar_toupper(cunichar c)
{
    int t = TYPE (c);
    if (t == C_UNICODE_LOWERCASE_LETTER) {
        cunichar val = ATTTABLE (c >> 8, c & 0xff);
        if (val >= 0x1000000) {
            const cchar *p = gsSpecialCaseTable + val - 0x1000000;
            val = c_utf8_get_char (p);
        }

        return val ? val : c;
    }
    else if (t == C_UNICODE_TITLECASE_LETTER) {
        unsigned int i;
        for (i = 0; i < C_N_ELEMENTS (gsTitleTable); ++i) {
            if (gsTitleTable[i][0] == c) {
                return gsTitleTable[i][1] ? gsTitleTable[i][1] : c;
            }
        }
    }
    return c;
}

cunichar c_unichar_tolower(cunichar c)
{
    int t = TYPE (c);
    if (t == C_UNICODE_UPPERCASE_LETTER) {
        cunichar val = ATTTABLE (c >> 8, c & 0xff);
        if (val >= 0x1000000) {
            const cchar *p = gsSpecialCaseTable + val - 0x1000000;
            return c_utf8_get_char (p);
        }
        else {
            return val ? val : c;
        }
    }
    else if (t == C_UNICODE_TITLECASE_LETTER) {
        unsigned int i;
        for (i = 0; i < C_N_ELEMENTS (gsTitleTable); ++i) {
            if (gsTitleTable[i][0] == c) {
                return gsTitleTable[i][2];
            }
        }
    }
    return c;
}

cunichar c_unichar_totitle(cunichar c)
{
    unsigned int i;

    if (c == 0) {
        return c;
    }

    for (i = 0; i < C_N_ELEMENTS (gsTitleTable); ++i) {
        if (gsTitleTable[i][0] == c
            || gsTitleTable[i][1] == c
            || gsTitleTable[i][2] == c) {
            return gsTitleTable[i][0];
        }
    }

    if (TYPE (c) == C_UNICODE_LOWERCASE_LETTER) {
        return c_unichar_toupper (c);
    }

    return c;
}

cint c_unichar_digit_value(cunichar c)
{
    if (TYPE (c) == C_UNICODE_DECIMAL_NUMBER) {
        return ATTTABLE (c >> 8, c & 0xff);
    }
    return -1;
}

cint c_unichar_xdigit_value(cunichar c)
{
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= C_UNICHAR_FULLWIDTH_A && c <= C_UNICHAR_FULLWIDTH_F) {
        return c - C_UNICHAR_FULLWIDTH_A + 10;
    }

    if (c >= C_UNICHAR_FULLWIDTH_a && c <= C_UNICHAR_FULLWIDTH_f) {
        return c - C_UNICHAR_FULLWIDTH_a + 10;
    }

    if (TYPE (c) == C_UNICODE_DECIMAL_NUMBER) {
        return ATTTABLE (c >> 8, c & 0xff);
    }

    return -1;
}

CUnicodeType c_unichar_type(cunichar c)
{
    return TYPE (c);
}

cchar *c_utf8_strup(const cchar *str, cssize len)
{
    csize result_len;
    LocaleType locale_type;
    cchar *result;

    c_return_val_if_fail (str != NULL, NULL);

    locale_type = get_locale_type ();

    result_len = real_toupper (str, len, NULL, locale_type);
    result = c_malloc0 (result_len + 1);
    real_toupper (str, len, result, locale_type);
    result[result_len] = '\0';

    return result;
}

cchar *c_utf8_strdown(const cchar *str, cssize len)
{
    csize result_len;
    LocaleType locale_type;
    cchar *result;

    c_return_val_if_fail (str != NULL, NULL);

    locale_type = get_locale_type ();

    result_len = real_tolower (str, len, NULL, locale_type);
    result = c_malloc0 (result_len + 1);
    real_tolower (str, len, result, locale_type);
    result[result_len] = '\0';

    return result;
}

cchar *c_utf8_casefold(const cchar *str, cssize len)
{
    CString *result;
    const char *p;

    c_return_val_if_fail (str != NULL, NULL);

    result = c_string_new (NULL);
    p = str;
    while ((len < 0 || p < str + len) && *p) {
        cunichar ch = c_utf8_get_char (p);

        int start = 0;
        int end = C_N_ELEMENTS (gsCaseFoldTable);
        if (ch >= gsCaseFoldTable[start].ch && ch <= gsCaseFoldTable[end - 1].ch) {
            while (true) {
                int half = (start + end) / 2;
                if (ch == gsCaseFoldTable[half].ch) {
                    c_string_append (result, gsCaseFoldTable[half].data);
                    goto next;
                }
                else if (half == start) {
                    break;
                }
                else if (ch > gsCaseFoldTable[half].ch) {
                    start = half;
                }
                else {
                    end = half;
                }
            }
        }
        c_string_append_unichar (result, c_unichar_tolower (ch));
next:
        p = c_utf8_next_char (p);
    }

    return c_string_free (result, false);
}

bool c_unichar_get_mirror_char(cunichar ch, cunichar *mirrored_ch)
{
    bool found;
    cunichar mirrored;

    mirrored = CLIB_GET_MIRRORING(ch);

    found = ch != mirrored;
    if (mirrored_ch) {
        *mirrored_ch = mirrored;
    }

    return found;
}

CUnicodeScript c_unichar_get_script(cunichar ch)
{
    if (ch < C_EASY_SCRIPTS_RANGE) {
        return gsScriptEasyTable[ch];
    }
    else {
        return c_unichar_get_script_bsearch (ch);
    }
}

cuint32 c_unicode_script_to_iso15924(CUnicodeScript script)
{
    c_return_val_if_fail((C_UNICODE_SCRIPT_INVALID_CODE != script), 0);

    if (C_UNLIKELY (script < 0 || script >= (int) C_N_ELEMENTS (gsISO15924Tags))) {
        return 0x5A7A7A7A;
    }

    return gsISO15924Tags[script];
}

CUnicodeScript c_unicode_script_from_iso15924(cuint32 iso15924)
{
    unsigned int i;

    if (!iso15924)
        return C_UNICODE_SCRIPT_INVALID_CODE;

    for (i = 0; i < C_N_ELEMENTS (gsISO15924Tags); i++) {
        if (gsISO15924Tags[i] == iso15924) {
            return (CUnicodeScript) i;
        }
    }

    return C_UNICODE_SCRIPT_UNKNOWN;
}


static inline cunichar c_utf8_get_char_extended (const cchar *p, cssize max_len)
{
    csize i, len;
    cunichar min_code;
    cunichar wc = (cuchar) *p;
    const cunichar partial_sequence = (cunichar) -2;
    const cunichar malformed_sequence = (cunichar) -1;

    if (wc < 0x80) {
        return wc;
    }
    else if (C_UNLIKELY (wc < 0xc0)) {
        return malformed_sequence;
    }
    else if (wc < 0xe0) {
        len = 2;
        wc &= 0x1f;
        min_code = 1 << 7;
    }
    else if (wc < 0xf0) {
        len = 3;
        wc &= 0x0f;
        min_code = 1 << 11;
    }
    else if (wc < 0xf8) {
        len = 4;
        wc &= 0x07;
        min_code = 1 << 16;
    }
    else if (wc < 0xfc) {
        len = 5;
        wc &= 0x03;
        min_code = 1 << 21;
    }
    else if (wc < 0xfe) {
        len = 6;
        wc &= 0x01;
        min_code = 1 << 26;
    }
    else {
        return malformed_sequence;
    }

    if (C_UNLIKELY (max_len >= 0 && len > (csize) max_len)) {
        for (i = 1; i < (csize) max_len; i++) {
            if ((((cuchar*)p)[i] & 0xc0) != 0x80) {
                return malformed_sequence;
            }
        }
        return partial_sequence;
    }

    for (i = 1; i < len; ++i) {
        cunichar ch = ((cuchar*)p)[i];

        if (C_UNLIKELY ((ch & 0xc0) != 0x80)) {
            if (ch) {
                return malformed_sequence;
            }
            else {
                return partial_sequence;
            }
        }

        wc <<= 6;
        wc |= (ch & 0x3f);
    }

    if (C_UNLIKELY (wc < min_code)) {
        return malformed_sequence;
    }

    return wc;
}

static void* try_malloc_n (csize n_blocks, csize n_block_bytes, CError **error)
{
    void* ptr = c_malloc0(n_blocks * n_block_bytes);
    if (ptr == NULL) {
        c_set_error_literal (error, C_CONVERT_ERROR, C_CONVERT_ERROR_NO_MEMORY, _("Failed to allocate memory"));
    }

    return ptr;
}

static const cchar* fast_validate (const char *str)
{
    const cchar *p;
    for (p = str; *p; p++) {
        if (*(cuchar*) p < 128) {
            /* done */;
        }
        else {
            const cchar *last;
            last = p;
            if (*(cuchar*)p < 0xe0) {
                /* 110xxxxx */
                if (C_UNLIKELY (*(cuchar*)p < 0xc2)) {
                    goto error;
                }
            }
            else {
                if (*(cuchar*)p < 0xf0) {
                    /* 1110xxxx */
                    switch (*(cuchar*)p++ & 0x0f) {
                        case 0:
                            VALIDATE_BYTE(0xe0, 0xa0); /* 0xa0 ... 0xbf */
                            break;
                        case 0x0d:
                            VALIDATE_BYTE(0xe0, 0x80); /* 0x80 ... 0x9f */
                            break;
                        default:
                            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                    }
                }
                else if (*(cuchar *)p < 0xf5) {
                    /* 11110xxx excluding out-of-range */
                    switch (*(cuchar*)p++ & 0x07) {
                        case 0:
                            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                            if (C_UNLIKELY((*(cuchar*)p & 0x30) == 0)) {
                                goto error;
                            }
                            break;
                        case 4:
                            VALIDATE_BYTE(0xf0, 0x80); /* 0x80 ... 0x8f */
                            break;
                        default:
                            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                    }
                    p++;
                    VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                }
                else
                    goto error;
            }

            p++;
            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */

            continue;

error:
            return last;
        }
    }

    return p;
}

static const cchar* fast_validate_len (const char *str, cssize max_len)
{
    const cchar *p;

    c_assert (max_len >= 0);
    for (p = str; ((p - str) < max_len) && *p; p++) {
        if (*(cuchar*)p < 128) {
            /* done */;
        }
        else {
            const cchar *last;
            last = p;
            if (*(cuchar*)p < 0xe0) {
                /* 110xxxxx */
                if (C_UNLIKELY (max_len - (p - str) < 2)) {
                    goto error;
                }

                if (C_UNLIKELY (*(cuchar*)p < 0xc2)) {
                    goto error;
                }
            }
            else {
                if (*(cuchar*)p < 0xf0) {
                    /* 1110xxxx */
                    if (C_UNLIKELY (max_len - (p - str) < 3)) {
                        goto error;
                    }

                    switch (*(cuchar*)p++ & 0x0f) {
                        case 0:
                            VALIDATE_BYTE(0xe0, 0xa0); /* 0xa0 ... 0xbf */
                            break;
                        case 0x0d:
                            VALIDATE_BYTE(0xe0, 0x80); /* 0x80 ... 0x9f */
                            break;
                        default:
                            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                    }
                }
                else if (*(cuchar*)p < 0xf5) {
                    /* 11110xxx excluding out-of-range */
                    if (C_UNLIKELY (max_len - (p - str) < 4)) {
                        goto error;
                    }

                    switch (*(cuchar *)p++ & 0x07) {
                        case 0:
                            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                            if (C_UNLIKELY((*(cuchar*)p & 0x30) == 0)) {
                                goto error;
                            }
                            break;
                        case 4:
                            VALIDATE_BYTE(0xf0, 0x80); /* 0x80 ... 0x8f */
                            break;
                        default:
                            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                    }
                    p++;
                    VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */
                }
                else {
                    goto error;
                }
            }

            p++;
            VALIDATE_BYTE(0xc0, 0x80); /* 10xxxxxx */

            continue;
error:
            return last;
        }
    }

    return p;
}

static void decompose_hangul (cunichar s, cunichar *r, csize* result_len)
{
    cint SIndex = s - SBase;
    cint TIndex = SIndex % TCount;

    if (r) {
        r[0] = LBase + SIndex / NCount;
        r[1] = VBase + (SIndex % NCount) / TCount;
    }

    if (TIndex) {
        if (r) {
            r[2] = TBase + TIndex;
        }
        *result_len = 3;
    }
    else {
        *result_len = 2;
    }
}

static const cchar* find_decomposition (cunichar ch, bool compat)
{
    int start = 0;
    int end = C_N_ELEMENTS (gsDecompTable);

    if (ch >= gsDecompTable[start].ch && ch <= gsDecompTable[end - 1].ch) {
        while (true) {
            int half = (start + end) / 2;
            if (ch == gsDecompTable[half].ch) {
                int offset;

                if (compat) {
                    offset = gsDecompTable[half].compatOffset;
                    if (offset == C_UNICODE_NOT_PRESENT_OFFSET)
                        offset = gsDecompTable[half].canonOffset;
                }
                else {
                    offset = gsDecompTable[half].canonOffset;
                    if (offset == C_UNICODE_NOT_PRESENT_OFFSET) {
                        return NULL;
                    }
                }
                return &(gsDecompExpansionString[offset]);
            }
            else if (half == start) {
                break;
            }
            else if (ch > gsDecompTable[half].ch) {
                start = half;
            }
            else {
                end = half;
            }
        }
    }

    return NULL;
}

static bool combine_hangul (cunichar a, cunichar b, cunichar *result)
{
    cint LIndex = a - LBase;
    cint SIndex = a - SBase;

    cint VIndex = b - VBase;
    cint TIndex = b - TBase;

    if (0 <= LIndex && LIndex < LCount && 0 <= VIndex && VIndex < VCount) {
        *result = SBase + (LIndex * VCount + VIndex) * TCount;
        return true;
    }
    else if (0 <= SIndex && SIndex < SCount && (SIndex % TCount) == 0 && 0 < TIndex && TIndex < TCount) {
        *result = a + TIndex;
        return true;
    }

    return false;
}

static bool combine (cunichar a, cunichar b, cunichar *result)
{
    cushort index_a, index_b;

    if (combine_hangul (a, b, result)) {
        return true;
    }

    index_a = COMPOSE_INDEX(a);

    if (index_a >= COMPOSE_FIRST_SINGLE_START && index_a < COMPOSE_SECOND_START) {
        if (b == gsComposeFirstSingle[index_a - COMPOSE_FIRST_SINGLE_START][0]) {
            *result = gsComposeFirstSingle[index_a - COMPOSE_FIRST_SINGLE_START][1];
            return true;
        }
        else
            return false;
    }

    index_b = COMPOSE_INDEX(b);

    if (index_b >= COMPOSE_SECOND_SINGLE_START) {
        if (a == gsComposeSecondSingle[index_b - COMPOSE_SECOND_SINGLE_START][0]) {
            *result = gsComposeSecondSingle[index_b - COMPOSE_SECOND_SINGLE_START][1];
            return true;
        }
        else {
            return false;
        }
    }

    if (index_a >= COMPOSE_FIRST_START && index_a < COMPOSE_FIRST_SINGLE_START &&
        index_b >= COMPOSE_SECOND_START && index_b < COMPOSE_SECOND_SINGLE_START) {
        cunichar res = gsComposeArray[index_a - COMPOSE_FIRST_START][index_b - COMPOSE_SECOND_START];
        if (res) {
            *result = res;
            return true;
        }
    }

    return false;
}

cunichar* _c_utf8_normalize_wc (const cchar* str, cssize max_len, CNormalizeMode mode)
{
    csize n_wc;
    cunichar *wc_buffer;
    const char *p;
    csize last_start;
    bool do_compat = (mode == C_NORMALIZE_NFKC || mode == C_NORMALIZE_NFKD);
    bool do_compose = (mode == C_NORMALIZE_NFC || mode == C_NORMALIZE_NFKC);

    n_wc = 0;
    p = str;
    while ((max_len < 0 || p < str + max_len) && *p) {
        const cchar *decomp;
        cunichar wc = c_utf8_get_char (p);
        if (wc >= SBase && wc < SBase + SCount) {
            csize result_len;
            decompose_hangul (wc, NULL, &result_len);
            n_wc += result_len;
        }
        else {
            decomp = find_decomposition (wc, do_compat);
            if (decomp) {
                n_wc += c_utf8_strlen (decomp, -1);
            }
            else {
                n_wc++;
            }
        }
        p = c_utf8_next_char (p);
    }

    wc_buffer = c_malloc0(sizeof(cunichar) * (n_wc + 1));

    last_start = 0;
    n_wc = 0;
    p = str;
    while ((max_len < 0 || p < str + max_len) && *p) {
        cunichar wc = c_utf8_get_char (p);
        const cchar *decomp;
        int cc;
        csize old_n_wc = n_wc;

        if (wc >= SBase && wc < SBase + SCount) {
            csize result_len;
            decompose_hangul (wc, wc_buffer + n_wc, &result_len);
            n_wc += result_len;
        }
        else {
            decomp = find_decomposition (wc, do_compat);
            if (decomp) {
                const char *pd;
                for (pd = decomp; *pd != '\0'; pd = c_utf8_next_char (pd)) {
                    wc_buffer[n_wc++] = c_utf8_get_char (pd);
                }
            }
            else {
                wc_buffer[n_wc++] = wc;
            }
        }

        if (n_wc > 0) {
            cc = COMBINING_CLASS (wc_buffer[old_n_wc]);
            if (cc == 0) {
                c_unicode_canonical_ordering (wc_buffer + last_start, n_wc - last_start);
                last_start = old_n_wc;
            }
        }
        p = c_utf8_next_char (p);
    }

    if (n_wc > 0) {
        c_unicode_canonical_ordering (wc_buffer + last_start, n_wc - last_start);
        last_start = n_wc;
        (void) last_start;
    }

    wc_buffer[n_wc] = 0;

    /* All decomposed and reordered */
    if (do_compose && n_wc > 0) {
        csize i, j;
        int last_cc = 0;
        last_start = 0;

        for (i = 0; i < n_wc; i++) {
            int cc = COMBINING_CLASS (wc_buffer[i]);

            if (i > 0 && (last_cc == 0 || last_cc < cc) && combine (wc_buffer[last_start], wc_buffer[i], &wc_buffer[last_start])) {
                for (j = i + 1; j < n_wc; j++) {
                    wc_buffer[j-1] = wc_buffer[j];
                }
                n_wc--;
                i--;

                if (i == last_start) {
                    last_cc = 0;
                }
                else {
                    last_cc = COMBINING_CLASS (wc_buffer[i-1]);
                }

                continue;
            }

            if (cc == 0) {
                last_start = i;
            }
            last_cc = cc;
        }
    }

    wc_buffer[n_wc] = 0;

    return wc_buffer;
}

static bool decompose_hangul_step (cunichar ch, cunichar* a, cunichar* b)
{
    cint SIndex, TIndex;

    if (ch < SBase || ch >= SBase + SCount) {
        return false;  /* not a hangul syllable */
    }

    SIndex = ch - SBase;
    TIndex = SIndex % TCount;

    if (TIndex) {
        /* split LVT -> LV,T */
        *a = ch - TIndex;
        *b = TBase + TIndex;
    }
    else {
        /* split LV -> L,V */
        *a = LBase + SIndex / NCount;
        *b = VBase + (SIndex % NCount) / TCount;
    }

    return true;
}

static int interval_compare (const void *key, const void *elt)
{
    cunichar c = C_POINTER_TO_UINT (key);
    struct Interval *interval = (struct Interval *)elt;

    if (c < interval->start) {
        return -1;
    }
    if (c > interval->end) {
        return +1;
    }

    return 0;
}

static inline bool c_unichar_iswide_bsearch (cunichar ch)
{
    int lower = 0;
    int upper = C_N_ELEMENTS (gsUnicodeWidthTableWide) - 1;
    static int saved_mid = C_WIDTH_TABLE_MIDPOINT;
    int mid = saved_mid;

    do {
        if (ch < gsUnicodeWidthTableWide[mid].start) {
            upper = mid - 1;
        }
        else if (ch > gsUnicodeWidthTableWide[mid].end) {
            lower = mid + 1;
        }
        else {
            return true;
        }

        mid = (lower + upper) / 2;
    }
    while (lower <= upper);

    return false;
}

static LocaleType get_locale_type (void)
{
    const char *locale = setlocale (LC_CTYPE, NULL);

    if (locale == NULL)
        return LOCALE_NORMAL;

    switch (locale[0])
    {
        case 'a':
            if (locale[1] == 'z')
                return LOCALE_TURKIC;
            break;
        case 'l':
            if (locale[1] == 't')
                return LOCALE_LITHUANIAN;
            break;
        case 't':
            if (locale[1] == 'r')
                return LOCALE_TURKIC;
            break;
    }

    return LOCALE_NORMAL;
}

static cint output_marks (const char **p_inout, char* out_buffer, bool remove_dot)
{
    const char *p = *p_inout;
    cint len = 0;

    while (*p) {
        cunichar c = c_utf8_get_char (p);
        if (UNICODE_ISMARK (TYPE (c))) {
            if (!remove_dot || c != 0x307 /* COMBINING DOT ABOVE */) {
                len += c_unichar_to_utf8 (c, out_buffer ? out_buffer + len : NULL);
            }
            p = c_utf8_next_char (p);
        }
        else {
            break;
        }
    }

    *p_inout = p;
    return len;
}
static cint output_special_case (cchar* out_buffer, int offset, int type, int which)
{
    const cchar *p = gsSpecialCaseTable + offset;
    cint len;

    if (type != C_UNICODE_TITLECASE_LETTER)
        p = c_utf8_next_char (p);

    if (which == 1)
        p += strlen (p) + 1;

    len = strlen (p);
    if (out_buffer)
        memcpy (out_buffer, p, len);

    return len;
}

static csize real_toupper (const cchar *str, cssize max_len, cchar* out_buffer, LocaleType locale_type)
{
    const cchar *p = str;
    const char *last = NULL;
    csize len = 0;
    bool last_was_i = false;

    while ((max_len < 0 || p < str + max_len) && *p) {
        cunichar c = c_utf8_get_char (p);
        int t = TYPE (c);
        cunichar val;

        last = p;
        p = c_utf8_next_char (p);
        if (locale_type == LOCALE_LITHUANIAN) {
            if (c == 'i') {
                last_was_i = true;
            }
            else {
                if (last_was_i) {
                    csize decomp_len, i;
                    cunichar decomp[C_UNICHAR_MAX_DECOMPOSITION_LENGTH];

                    decomp_len = c_unichar_fully_decompose (c, false, decomp, C_N_ELEMENTS (decomp));
                    for (i=0; i < decomp_len; i++) {
                        if (decomp[i] != 0x307 /* COMBINING DOT ABOVE */)
                            len += c_unichar_to_utf8 (c_unichar_toupper (decomp[i]), out_buffer ? out_buffer + len : NULL);
                    }

                    len += output_marks (&p, out_buffer ? out_buffer + len : NULL, true);

                    continue;
                }

                if (!UNICODE_ISMARK (t)) {
                    last_was_i = false;
                }
            }
        }

        if (locale_type == LOCALE_TURKIC && c == 'i') {
            /* i => LATIN CAPITAL LETTER I WITH DOT ABOVE */
            len += c_unichar_to_utf8 (0x130, out_buffer ? out_buffer + len : NULL);
        }
        else if (c == 0x0345) {
            /* COMBINING GREEK YPOGEGRAMMENI */
            /* Nasty, need to move it after other combining marks .. this would go away if
             * we normalized first.
             */
            len += output_marks (&p, out_buffer ? out_buffer + len : NULL, false);

            /* And output as GREEK CAPITAL LETTER IOTA */
            len += c_unichar_to_utf8 (0x399, out_buffer ? out_buffer + len : NULL);
        }
        else if (IS (t, OR (C_UNICODE_LOWERCASE_LETTER, OR (C_UNICODE_TITLECASE_LETTER, 0)))) {
            val = ATTTABLE (c >> 8, c & 0xff);
            if (val >= 0x1000000) {
                len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t, t == C_UNICODE_LOWERCASE_LETTER ? 0 : 1);
            }
            else {
                if (t == C_UNICODE_TITLECASE_LETTER) {
                    unsigned int i;
                    for (i = 0; i < C_N_ELEMENTS (gsTitleTable); ++i) {
                        if (gsTitleTable[i][0] == c) {
                            val = gsTitleTable[i][1];
                            break;
                        }
                    }
                }
                len += c_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
            }
        }
        else {
            csize char_len = gsUtf8Skip[*(cuchar *)last];
            if (out_buffer) {
                memcpy (out_buffer + len, last, char_len);
            }
            len += char_len;
        }
    }

    return len;
}

static bool has_more_above (const cchar *str)
{
    const cchar *p = str;
    cint combining_class;

    while (*p) {
        combining_class = c_unichar_combining_class (c_utf8_get_char (p));
        if (combining_class == 230)
            return true;
        else if (combining_class == 0)
            break;

        p = c_utf8_next_char (p);
    }

    return false;
}

static csize real_tolower (const cchar *str, cssize max_len, cchar* out_buffer, LocaleType locale_type)
{
    const cchar *p = str;
    const char *last = NULL;
    csize len = 0;

    while ((max_len < 0 || p < str + max_len) && *p) {
        cunichar c = c_utf8_get_char (p);
        int t = TYPE (c);
        cunichar val;

        last = p;
        p = c_utf8_next_char (p);

        if (locale_type == LOCALE_TURKIC && (c == 'I' || c == 0x130 || c == C_UNICHAR_FULLWIDTH_I)) {
            bool combining_dot = (c == 'I' || c == C_UNICHAR_FULLWIDTH_I) && c_utf8_get_char (p) == 0x0307;
            if (combining_dot || c == 0x130) {
                len += c_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL);
                if (combining_dot) {
                    p = c_utf8_next_char (p);
                }
            }
            else {
                len += c_unichar_to_utf8 (0x131, out_buffer ? out_buffer + len : NULL);
            }
        }
        else if (locale_type == LOCALE_LITHUANIAN && (c == 0x00cc || c == 0x00cd || c == 0x0128)) {
            len += c_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL);
            len += c_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL);
            switch (c) {
                case 0x00cc:
                    len += c_unichar_to_utf8 (0x0300, out_buffer ? out_buffer + len : NULL);
                    break;
                case 0x00cd:
                    len += c_unichar_to_utf8 (0x0301, out_buffer ? out_buffer + len : NULL);
                    break;
                case 0x0128:
                    len += c_unichar_to_utf8 (0x0303, out_buffer ? out_buffer + len : NULL);
                    break;
            }
        }
        else if (locale_type == LOCALE_LITHUANIAN
            && (c == 'I' || c == C_UNICHAR_FULLWIDTH_I || c == 'J' || c == C_UNICHAR_FULLWIDTH_J || c == 0x012e)
            && has_more_above (p)) {
            len += c_unichar_to_utf8 (c_unichar_tolower (c), out_buffer ? out_buffer + len : NULL);
            len += c_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL);
        }
        else if (c == 0x03A3) {
            /* GREEK CAPITAL LETTER SIGMA */
            if ((max_len < 0 || p < str + max_len) && *p) {
                cunichar next_c = c_utf8_get_char (p);
                int next_type = TYPE(next_c);

                /**
                 * SIGMA mapps differently depending on whether it is
                 * final or not. The following simplified test would
                 * fail in the case of combining marks following the
                 * sigma, but I don't think that occurs in real text.
                 * The test here matches that in ICU.
                 */
                if (ISALPHA (next_type)) /* Lu,Ll,Lt,Lm,Lo */
                    val = 0x3c3;    /* GREEK SMALL SIGMA */
                else
                    val = 0x3c2;    /* GREEK SMALL FINAL SIGMA */
            }
            else {
                val = 0x3c2;        /* GREEK SMALL FINAL SIGMA */
            }

            len += c_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
        }
        else if (IS (t, OR (C_UNICODE_UPPERCASE_LETTER, OR (C_UNICODE_TITLECASE_LETTER, 0)))) {
            val = ATTTABLE (c >> 8, c & 0xff);
            if (val >= 0x1000000) {
                len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t, 0);
            }
            else {
                if (t == C_UNICODE_TITLECASE_LETTER) {
                    unsigned int i;
                    for (i = 0; i < C_N_ELEMENTS (gsTitleTable); ++i) {
                        if (gsTitleTable[i][0] == c) {
                            val = gsTitleTable[i][2];
                            break;
                        }
                    }
                }
                len += c_unichar_to_utf8 (val ? val : c, out_buffer ? out_buffer + len : NULL);
            }
        }
        else {
            csize char_len = gsUtf8Skip[*(cuchar*) last];
            if (out_buffer) {
                memcpy (out_buffer + len, last, char_len);
            }
            len += char_len;
        }
    }

    return len;
}

static inline CUnicodeScript c_unichar_get_script_bsearch (cunichar ch)
{
    int lower = 0;
    int upper = C_N_ELEMENTS (gsScriptTable) - 1;
    static int saved_mid = C_SCRIPT_TABLE_MIDPOINT;
    int mid = saved_mid;

    do {
        if (ch < gsScriptTable[mid].start)
            upper = mid - 1;
        else if (ch >= gsScriptTable[mid].start + gsScriptTable[mid].chars)
            lower = mid + 1;
        else
            return gsScriptTable[saved_mid = mid].script;

        mid = (lower + upper) / 2;
    }
    while (lower <= upper);

    return C_UNICODE_SCRIPT_UNKNOWN;
}

