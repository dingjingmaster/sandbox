//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_UNISTR_H
#define sandbox_UNISTR_H
#include <c/clib.h>

#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C

extern bool fs_names_are_equal(const FSChar *s1, size_t s1_len, const FSChar *s2, size_t s2_len, const IGNORE_CASE_BOOL ic, const FSChar *upcase, const u32 upcase_size);
extern int fs_names_full_collate(const FSChar *name1, const u32 name1_len, const FSChar *name2, const u32 name2_len, const IGNORE_CASE_BOOL ic, const FSChar *upcase, const u32 upcase_len);
extern int fs_ucsncmp(const FSChar *s1, const FSChar *s2, size_t n);
extern int fs_ucsncasecmp(const FSChar *s1, const FSChar *s2, size_t n, const FSChar *upcase, const u32 upcase_size);
extern u32 fs_ucsnlen(const FSChar *s, u32 maxlen);
extern FSChar* fs_ucsndup(const FSChar *s, u32 maxlen);
extern void fs_name_upcase(FSChar *name, u32 name_len, const FSChar *upcase, const u32 upcase_len);
extern void fs_name_locase(FSChar *name, u32 name_len, const FSChar* locase, const u32 locase_len);
extern void fs_file_value_upcase(FILE_NAME_ATTR *file_name_attr,
const FSChar *upcase, const u32 upcase_len);
extern int fs_ucstombs(const FSChar *ins, const int ins_len, char **outs, int outs_len);
extern int fs_mbstoucs(const char *ins, FSChar **outs);

extern char *fs_uppercase_mbs(const char *low, const FSChar *upcase, u32 upcase_len);

extern void fs_upcase_table_build(FSChar *uc, u32 uc_len);
extern u32 fs_upcase_build_default(FSChar **upcase);
extern FSChar *fs_locase_table_build(const FSChar *uc, u32 uc_cnt);

extern FSChar *fs_str2ucs(const char *s, int *len);

extern void fs_ucsfree(FSChar *ucs);

extern bool fs_forbidden_chars(const FSChar *name, int len, bool strict);
extern bool fs_forbidden_names(FSVolume *vol, const FSChar *name, int len, bool strict);
extern bool fs_collapsible_chars(FSVolume *vol, const FSChar *shortname, int shortlen, const FSChar *longname, int longlen);

extern int fs_set_char_encoding(const char *locale);

C_END_EXTERN_C

#endif // sandbox_UNISTR_H
