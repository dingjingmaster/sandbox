
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

#ifndef clibrary_CLIBRARY_UNICODE_H
#define clibrary_CLIBRARY_UNICODE_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

#define C_UNICHAR_MAX_DECOMPOSITION_LENGTH 18
#define C_UNICODE_COMBINING_MARK C_UNICODE_SPACING_MARK
#define c_utf8_next_char(p) (char*)((p) + gsUtf8Skip[*(const cuchar*)(p)])

typedef enum
{
    C_UNICODE_CONTROL,
    C_UNICODE_FORMAT,
    C_UNICODE_UNASSIGNED,
    C_UNICODE_PRIVATE_USE,
    C_UNICODE_SURROGATE,
    C_UNICODE_LOWERCASE_LETTER,
    C_UNICODE_MODIFIER_LETTER,
    C_UNICODE_OTHER_LETTER,
    C_UNICODE_TITLECASE_LETTER,
    C_UNICODE_UPPERCASE_LETTER,
    C_UNICODE_SPACING_MARK,
    C_UNICODE_ENCLOSING_MARK,
    C_UNICODE_NON_SPACING_MARK,
    C_UNICODE_DECIMAL_NUMBER,
    C_UNICODE_LETTER_NUMBER,
    C_UNICODE_OTHER_NUMBER,
    C_UNICODE_CONNECT_PUNCTUATION,
    C_UNICODE_DASH_PUNCTUATION,
    C_UNICODE_CLOSE_PUNCTUATION,
    C_UNICODE_FINAL_PUNCTUATION,
    C_UNICODE_INITIAL_PUNCTUATION,
    C_UNICODE_OTHER_PUNCTUATION,
    C_UNICODE_OPEN_PUNCTUATION,
    C_UNICODE_CURRENCY_SYMBOL,
    C_UNICODE_MODIFIER_SYMBOL,
    C_UNICODE_MATH_SYMBOL,
    C_UNICODE_OTHER_SYMBOL,
    C_UNICODE_LINE_SEPARATOR,
    C_UNICODE_PARAGRAPH_SEPARATOR,
    C_UNICODE_SPACE_SEPARATOR
} CUnicodeType;

typedef enum
{
    C_UNICODE_BREAK_MANDATORY,
    C_UNICODE_BREAK_CARRIAGE_RETURN,
    C_UNICODE_BREAK_LINE_FEED,
    C_UNICODE_BREAK_COMBINING_MARK,
    C_UNICODE_BREAK_SURROGATE,
    C_UNICODE_BREAK_ZERO_WIDTH_SPACE,
    C_UNICODE_BREAK_INSEPARABLE,
    C_UNICODE_BREAK_NON_BREAKING_GLUE,
    C_UNICODE_BREAK_CONTINGENT,
    C_UNICODE_BREAK_SPACE,
    C_UNICODE_BREAK_AFTER,
    C_UNICODE_BREAK_BEFORE,
    C_UNICODE_BREAK_BEFORE_AND_AFTER,
    C_UNICODE_BREAK_HYPHEN,
    C_UNICODE_BREAK_NON_STARTER,
    C_UNICODE_BREAK_OPEN_PUNCTUATION,
    C_UNICODE_BREAK_CLOSE_PUNCTUATION,
    C_UNICODE_BREAK_QUOTATION,
    C_UNICODE_BREAK_EXCLAMATION,
    C_UNICODE_BREAK_IDEOGRAPHIC,
    C_UNICODE_BREAK_NUMERIC,
    C_UNICODE_BREAK_INFIX_SEPARATOR,
    C_UNICODE_BREAK_SYMBOL,
    C_UNICODE_BREAK_ALPHABETIC,
    C_UNICODE_BREAK_PREFIX,
    C_UNICODE_BREAK_POSTFIX,
    C_UNICODE_BREAK_COMPLEX_CONTEXT,
    C_UNICODE_BREAK_AMBIGUOUS,
    C_UNICODE_BREAK_UNKNOWN,
    C_UNICODE_BREAK_NEXT_LINE,
    C_UNICODE_BREAK_WORD_JOINER,
    C_UNICODE_BREAK_HANGUL_L_JAMO,
    C_UNICODE_BREAK_HANGUL_V_JAMO,
    C_UNICODE_BREAK_HANGUL_T_JAMO,
    C_UNICODE_BREAK_HANGUL_LV_SYLLABLE,
    C_UNICODE_BREAK_HANGUL_LVT_SYLLABLE,
    C_UNICODE_BREAK_CLOSE_PARANTHESIS,
    C_UNICODE_BREAK_CLOSE_PARENTHESIS = C_UNICODE_BREAK_CLOSE_PARANTHESIS,
    C_UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER,
    C_UNICODE_BREAK_HEBREW_LETTER,
    C_UNICODE_BREAK_REGIONAL_INDICATOR,
    C_UNICODE_BREAK_EMOJI_BASE,
    C_UNICODE_BREAK_EMOJI_MODIFIER,
    C_UNICODE_BREAK_ZERO_WIDTH_JOINER
} CUnicodeBreakType;

typedef enum
{                         /* ISO 15924 code */
    C_UNICODE_SCRIPT_INVALID_CODE = -1,
    C_UNICODE_SCRIPT_COMMON       = 0,   /* Zyyy */
    C_UNICODE_SCRIPT_INHERITED,          /* Zinh (Qaai) */
    C_UNICODE_SCRIPT_ARABIC,             /* Arab */
    C_UNICODE_SCRIPT_ARMENIAN,           /* Armn */
    C_UNICODE_SCRIPT_BENGALI,            /* Beng */
    C_UNICODE_SCRIPT_BOPOMOFO,           /* Bopo */
    C_UNICODE_SCRIPT_CHEROKEE,           /* Cher */
    C_UNICODE_SCRIPT_COPTIC,             /* Copt (Qaac) */
    C_UNICODE_SCRIPT_CYRILLIC,           /* Cyrl (Cyrs) */
    C_UNICODE_SCRIPT_DESERET,            /* Dsrt */
    C_UNICODE_SCRIPT_DEVANAGARI,         /* Deva */
    C_UNICODE_SCRIPT_ETHIOPIC,           /* Ethi */
    C_UNICODE_SCRIPT_GEORGIAN,           /* Geor (Geon, Geoa) */
    C_UNICODE_SCRIPT_GOTHIC,             /* Goth */
    C_UNICODE_SCRIPT_GREEK,              /* Grek */
    C_UNICODE_SCRIPT_GUJARATI,           /* Gujr */
    C_UNICODE_SCRIPT_GURMUKHI,           /* Guru */
    C_UNICODE_SCRIPT_HAN,                /* Hani */
    C_UNICODE_SCRIPT_HANGUL,             /* Hang */
    C_UNICODE_SCRIPT_HEBREW,             /* Hebr */
    C_UNICODE_SCRIPT_HIRAGANA,           /* Hira */
    C_UNICODE_SCRIPT_KANNADA,            /* Knda */
    C_UNICODE_SCRIPT_KATAKANA,           /* Kana */
    C_UNICODE_SCRIPT_KHMER,              /* Khmr */
    C_UNICODE_SCRIPT_LAO,                /* Laoo */
    C_UNICODE_SCRIPT_LATIN,              /* Latn (Latf, Latg) */
    C_UNICODE_SCRIPT_MALAYALAM,          /* Mlym */
    C_UNICODE_SCRIPT_MONGOLIAN,          /* Mong */
    C_UNICODE_SCRIPT_MYANMAR,            /* Mymr */
    C_UNICODE_SCRIPT_OGHAM,              /* Ogam */
    C_UNICODE_SCRIPT_OLD_ITALIC,         /* Ital */
    C_UNICODE_SCRIPT_ORIYA,              /* Orya */
    C_UNICODE_SCRIPT_RUNIC,              /* Runr */
    C_UNICODE_SCRIPT_SINHALA,            /* Sinh */
    C_UNICODE_SCRIPT_SYRIAC,             /* Syrc (Syrj, Syrn, Syre) */
    C_UNICODE_SCRIPT_TAMIL,              /* Taml */
    C_UNICODE_SCRIPT_TELUGU,             /* Telu */
    C_UNICODE_SCRIPT_THAANA,             /* Thaa */
    C_UNICODE_SCRIPT_THAI,               /* Thai */
    C_UNICODE_SCRIPT_TIBETAN,            /* Tibt */
    C_UNICODE_SCRIPT_CANADIAN_ABORIGINAL, /* Cans */
    C_UNICODE_SCRIPT_YI,                 /* Yiii */
    C_UNICODE_SCRIPT_TAGALOG,            /* Tglg */
    C_UNICODE_SCRIPT_HANUNOO,            /* Hano */
    C_UNICODE_SCRIPT_BUHID,              /* Buhd */
    C_UNICODE_SCRIPT_TAGBANWA,           /* Tagb */

    /* Unicode-4.0 additions */
    C_UNICODE_SCRIPT_BRAILLE,            /* Brai */
    C_UNICODE_SCRIPT_CYPRIOT,            /* Cprt */
    C_UNICODE_SCRIPT_LIMBU,              /* Limb */
    C_UNICODE_SCRIPT_OSMANYA,            /* Osma */
    C_UNICODE_SCRIPT_SHAVIAN,            /* Shaw */
    C_UNICODE_SCRIPT_LINEAR_B,           /* Linb */
    C_UNICODE_SCRIPT_TAI_LE,             /* Tale */
    C_UNICODE_SCRIPT_UGARITIC,           /* Ugar */

    /* Unicode-4.1 additions */
    C_UNICODE_SCRIPT_NEW_TAI_LUE,        /* Talu */
    C_UNICODE_SCRIPT_BUGINESE,           /* Bugi */
    C_UNICODE_SCRIPT_GLAGOLITIC,         /* Glag */
    C_UNICODE_SCRIPT_TIFINAGH,           /* Tfng */
    C_UNICODE_SCRIPT_SYLOTI_NAGRI,       /* Sylo */
    C_UNICODE_SCRIPT_OLD_PERSIAN,        /* Xpeo */
    C_UNICODE_SCRIPT_KHAROSHTHI,         /* Khar */

    /* Unicode-5.0 additions */
    C_UNICODE_SCRIPT_UNKNOWN,            /* Zzzz */
    C_UNICODE_SCRIPT_BALINESE,           /* Bali */
    C_UNICODE_SCRIPT_CUNEIFORM,          /* Xsux */
    C_UNICODE_SCRIPT_PHOENICIAN,         /* Phnx */
    C_UNICODE_SCRIPT_PHAGS_PA,           /* Phag */
    C_UNICODE_SCRIPT_NKO,                /* Nkoo */

    /* Unicode-5.1 additions */
    C_UNICODE_SCRIPT_KAYAH_LI,           /* Kali */
    C_UNICODE_SCRIPT_LEPCHA,             /* Lepc */
    C_UNICODE_SCRIPT_REJANG,             /* Rjng */
    C_UNICODE_SCRIPT_SUNDANESE,          /* Sund */
    C_UNICODE_SCRIPT_SAURASHTRA,         /* Saur */
    C_UNICODE_SCRIPT_CHAM,               /* Cham */
    C_UNICODE_SCRIPT_OL_CHIKI,           /* Olck */
    C_UNICODE_SCRIPT_VAI,                /* Vaii */
    C_UNICODE_SCRIPT_CARIAN,             /* Cari */
    C_UNICODE_SCRIPT_LYCIAN,             /* Lyci */
    C_UNICODE_SCRIPT_LYDIAN,             /* Lydi */

    /* Unicode-5.2 additions */
    C_UNICODE_SCRIPT_AVESTAN,                /* Avst */
    C_UNICODE_SCRIPT_BAMUM,                  /* Bamu */
    C_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS,   /* Egyp */
    C_UNICODE_SCRIPT_IMPERIAL_ARAMAIC,       /* Armi */
    C_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI,  /* Phli */
    C_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN, /* Prti */
    C_UNICODE_SCRIPT_JAVANESE,               /* Java */
    C_UNICODE_SCRIPT_KAITHI,                 /* Kthi */
    C_UNICODE_SCRIPT_LISU,                   /* Lisu */
    C_UNICODE_SCRIPT_MEETEI_MAYEK,           /* Mtei */
    C_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN,      /* Sarb */
    C_UNICODE_SCRIPT_OLD_TURKIC,             /* Orkh */
    C_UNICODE_SCRIPT_SAMARITAN,              /* Samr */
    C_UNICODE_SCRIPT_TAI_THAM,               /* Lana */
    C_UNICODE_SCRIPT_TAI_VIET,               /* Tavt */

    /* Unicode-6.0 additions */
    C_UNICODE_SCRIPT_BATAK,                  /* Batk */
    C_UNICODE_SCRIPT_BRAHMI,                 /* Brah */
    C_UNICODE_SCRIPT_MANDAIC,                /* Mand */

    /* Unicode-6.1 additions */
    C_UNICODE_SCRIPT_CHAKMA,                 /* Cakm */
    C_UNICODE_SCRIPT_MEROITIC_CURSIVE,       /* Merc */
    C_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS,   /* Mero */
    C_UNICODE_SCRIPT_MIAO,                   /* Plrd */
    C_UNICODE_SCRIPT_SHARADA,                /* Shrd */
    C_UNICODE_SCRIPT_SORA_SOMPENG,           /* Sora */
    C_UNICODE_SCRIPT_TAKRI,                  /* Takr */

    /* Unicode 7.0 additions */
    C_UNICODE_SCRIPT_BASSA_VAH,              /* Bass */
    C_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN,     /* Aghb */
    C_UNICODE_SCRIPT_DUPLOYAN,               /* Dupl */
    C_UNICODE_SCRIPT_ELBASAN,                /* Elba */
    C_UNICODE_SCRIPT_GRANTHA,                /* Gran */
    C_UNICODE_SCRIPT_KHOJKI,                 /* Khoj */
    C_UNICODE_SCRIPT_KHUDAWADI,              /* Sind */
    C_UNICODE_SCRIPT_LINEAR_A,               /* Lina */
    C_UNICODE_SCRIPT_MAHAJANI,               /* Mahj */
    C_UNICODE_SCRIPT_MANICHAEAN,             /* Mani */
    C_UNICODE_SCRIPT_MENDE_KIKAKUI,          /* Mend */
    C_UNICODE_SCRIPT_MODI,                   /* Modi */
    C_UNICODE_SCRIPT_MRO,                    /* Mroo */
    C_UNICODE_SCRIPT_NABATAEAN,              /* Nbat */
    C_UNICODE_SCRIPT_OLD_NORTH_ARABIAN,      /* Narb */
    C_UNICODE_SCRIPT_OLD_PERMIC,             /* Perm */
    C_UNICODE_SCRIPT_PAHAWH_HMONG,           /* Hmng */
    C_UNICODE_SCRIPT_PALMYRENE,              /* Palm */
    C_UNICODE_SCRIPT_PAU_CIN_HAU,            /* Pauc */
    C_UNICODE_SCRIPT_PSALTER_PAHLAVI,        /* Phlp */
    C_UNICODE_SCRIPT_SIDDHAM,                /* Sidd */
    C_UNICODE_SCRIPT_TIRHUTA,                /* Tirh */
    C_UNICODE_SCRIPT_WARANG_CITI,            /* Wara */

    /* Unicode 8.0 additions */
    C_UNICODE_SCRIPT_AHOM,                   /* Ahom */
    C_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS,  /* Hluw */
    C_UNICODE_SCRIPT_HATRAN,                 /* Hatr */
    C_UNICODE_SCRIPT_MULTANI,                /* Mult */
    C_UNICODE_SCRIPT_OLD_HUNGARIAN,          /* Hung */
    C_UNICODE_SCRIPT_SIGNWRITING,            /* Sgnw */

    /* Unicode 9.0 additions */
    C_UNICODE_SCRIPT_ADLAM,                  /* Adlm */
    C_UNICODE_SCRIPT_BHAIKSUKI,              /* Bhks */
    C_UNICODE_SCRIPT_MARCHEN,                /* Marc */
    C_UNICODE_SCRIPT_NEWA,                   /* Newa */
    C_UNICODE_SCRIPT_OSAGE,                  /* Osge */
    C_UNICODE_SCRIPT_TANGUT,                 /* Tang */

    /* Unicode 10.0 additions */
    C_UNICODE_SCRIPT_MASARAM_GONDI,          /* Gonm */
    C_UNICODE_SCRIPT_NUSHU,                  /* Nshu */
    C_UNICODE_SCRIPT_SOYOMBO,                /* Soyo */
    C_UNICODE_SCRIPT_ZANABAZAR_SQUARE,       /* Zanb */

    /* Unicode 11.0 additions */
    C_UNICODE_SCRIPT_DOGRA,                  /* Dogr */
    C_UNICODE_SCRIPT_GUNJALA_GONDI,          /* Gong */
    C_UNICODE_SCRIPT_HANIFI_ROHINGYA,        /* Rohg */
    C_UNICODE_SCRIPT_MAKASAR,                /* Maka */
    C_UNICODE_SCRIPT_MEDEFAIDRIN,            /* Medf */
    C_UNICODE_SCRIPT_OLD_SOGDIAN,            /* Sogo */
    C_UNICODE_SCRIPT_SOGDIAN,                /* Sogd */

    /* Unicode 12.0 additions */
    C_UNICODE_SCRIPT_ELYMAIC,                /* Elym */
    C_UNICODE_SCRIPT_NANDINAGARI,            /* Nand */
    C_UNICODE_SCRIPT_NYIAKENG_PUACHUE_HMONG, /* Rohg */
    C_UNICODE_SCRIPT_WANCHO,                 /* Wcho */

    /* Unicode 13.0 additions */
    C_UNICODE_SCRIPT_CHORASMIAN,             /* Chrs */
    C_UNICODE_SCRIPT_DIVES_AKURU,            /* Diak */
    C_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT,    /* Kits */
    C_UNICODE_SCRIPT_YEZIDI,                 /* Yezi */

    /* Unicode 14.0 additions */
    C_UNICODE_SCRIPT_CYPRO_MINOAN,           /* Cpmn */
    C_UNICODE_SCRIPT_OLD_UYGHUR,             /* Ougr */
    C_UNICODE_SCRIPT_TANGSA,                 /* Tnsa */
    C_UNICODE_SCRIPT_TOTO,                   /* Toto */
    C_UNICODE_SCRIPT_VITHKUQI,               /* Vith */

    /* not really a Unicode script, but part of ISO 15924 */
    C_UNICODE_SCRIPT_MATH,                   /* Zmth */

    /* Unicode 15.0 additions */
    C_UNICODE_SCRIPT_KAWI,          /* Kawi */
    C_UNICODE_SCRIPT_NAG_MUNDARI,   /* Nag Mundari */
} CUnicodeScript;

typedef enum
{
    C_NORMALIZE_DEFAULT,
    C_NORMALIZE_NFD = C_NORMALIZE_DEFAULT,
    C_NORMALIZE_DEFAULT_COMPOSE,
    C_NORMALIZE_NFC = C_NORMALIZE_DEFAULT_COMPOSE,
    C_NORMALIZE_ALL,
    C_NORMALIZE_NFKD = C_NORMALIZE_ALL,
    C_NORMALIZE_ALL_COMPOSE,
    C_NORMALIZE_NFKC = C_NORMALIZE_ALL_COMPOSE
} CNormalizeMode;


extern const char * const gsUtf8Skip;


bool        c_unichar_validate          (cunichar ch);
char*       c_utf8_prev_char            (const char* p);
cunichar    c_utf8_get_char             (const cchar* p);
clong       c_utf8_strlen               (const char* p, cssize max);
cint        c_unichar_to_utf8           (cunichar c, cchar* outbuf);
cchar*      c_utf8_strup                (const cchar *str, cssize len);
cchar*      c_utf8_make_valid           (const cchar* str, cssize len);
cchar*      c_utf8_strreverse           (const cchar* str, cssize len);
cunichar    c_utf8_get_char_validated   (const cchar *p, cssize maxLen);
char*       c_utf8_find_prev_char       (const char* str, const char* p);
char*       c_utf8_find_next_char       (const char* p, const char* end);
cchar*      c_utf8_offset_to_pointer    (const cchar* str, clong offset);
clong       c_utf8_pointer_to_offset    (const cchar* str, const cchar* pos);
cchar*      c_utf8_strrchr              (const char *p, cssize len, cunichar c);
cchar*      c_utf8_strchr               (const char *p, cssize len, cunichar c);
cchar*      c_utf8_strncpy              (cchar* dest, const cchar *src, csize n);
char*       c_utf8_substring            (const char* str, clong startPos, clong endPos);
cunichar*   c_utf8_to_ucs4_fast         (const cchar* str, clong len, clong* itemsWritten);
bool        c_utf8_validate_len         (const char* str, csize max_len, const cchar **end);
bool        c_utf8_validate             (const char* str, cssize max_len, const cchar **end);
cchar*      c_utf8_normalize            (const cchar* str, cssize len, CNormalizeMode mode);
cunichar*   c_utf8_to_ucs4              (const cchar* str, clong len, clong* itemsRead, clong* itemsWritten, CError** error);
cunichar2*  c_utf8_to_utf16             (const cchar* str, clong len, clong* itemsRead, clong* itemsWritten, CError** error);
cchar*      c_ucs4_to_utf8              (const cunichar* str, clong len, clong* itemsRead, clong* itemsWritten, CError** error);
cunichar2*  c_ucs4_to_utf16             (const cunichar* str, clong len, clong* itemsRead, clong* itemsWritten, CError** error);
cchar*      c_utf16_to_utf8             (const cunichar2* str, clong len, clong* itemsRead, clong* itemsWritten, CError** error);
cunichar*   c_utf16_to_ucs4             (const cunichar2* str, clong len, clong* itemsRead, clong* itemsWritten, CError** error);

bool            c_unichar_isalnum                   (cunichar c);
bool            c_unichar_isalpha                   (cunichar c);
bool            c_unichar_iscntrl                   (cunichar c);
bool            c_unichar_isdigit                   (cunichar c);
bool            c_unichar_isgraph                   (cunichar c);
bool            c_unichar_islower                   (cunichar c);
bool            c_unichar_isprint                   (cunichar c);
bool            c_unichar_ispunct                   (cunichar c);
bool            c_unichar_isspace                   (cunichar c);
bool            c_unichar_ismark                    (cunichar c);
bool            c_unichar_isupper                   (cunichar c);
bool            c_unichar_istitle                   (cunichar c);
bool            c_unichar_isxdigit                  (cunichar c);
bool            c_unichar_isdefined                 (cunichar c);
bool            c_unichar_iszerowidth               (cunichar c);
bool            c_unichar_iswide                    (cunichar c);
bool            c_unichar_iswide_cjk                (cunichar c);
cunichar        c_unichar_toupper                   (cunichar c);
cunichar        c_unichar_tolower                   (cunichar c);
cunichar        c_unichar_totitle                   (cunichar c);
cint            c_unichar_digit_value               (cunichar c);
cint            c_unichar_xdigit_value              (cunichar c);
cint            c_unichar_combining_class           (cunichar c);
CUnicodeType    c_unichar_type                      (cunichar c);
CUnicodeScript  c_unichar_get_script                (cunichar ch);
void            c_unicode_canonical_ordering        (cunichar* str, csize len);
cunichar*       c_unicode_canonical_decomposition   (cunichar ch, csize* resultLen);
bool            c_unichar_get_mirror_char           (cunichar ch, cunichar *mirrored_ch);
bool            c_unichar_compose                   (cunichar a, cunichar b, cunichar *ch);
bool            c_unichar_decompose                 (cunichar ch, cunichar* a, cunichar* b);
csize           c_unichar_fully_decompose           (cunichar ch, bool compat, cunichar *result, csize resultLen);

CUnicodeScript  c_unicode_script_from_iso15924      (cuint32 iso15924);
cuint32         c_unicode_script_to_iso15924        (CUnicodeScript script);



cchar*          c_utf8_strdown      (const cchar *str, cssize len);
cchar*          c_utf8_casefold     (const cchar* str, cssize len);


#endif // clibrary_CLIBRARY_UNICODE_H
