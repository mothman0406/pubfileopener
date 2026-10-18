// Small stand-in for the few ICU functions libmspub calls, so the WebAssembly build
// does not need the full ICU data (tens of MB). Handles UTF-16, UTF-8 and Windows-1252.
#include <unicode/utypes.h>
#include <unicode/utf.h>
#include <unicode/ucnv.h>
#include <unicode/ucsdet.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>
struct UConverter { int kind; };            // 1 utf8, 2 utf16le, 3 utf16be, 4 cp1252
struct UCharsetDetector { int unused; };
struct UCharsetMatch { const char *name; int32_t confidence; };
static const uint16_t kCp1252High[32] = {
  0x20AC,0xFFFD,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,0x02C6,0x2030,0x0160,0x2039,0x0152,0xFFFD,0x017D,0xFFFD,
  0xFFFD,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x02DC,0x2122,0x0161,0x203A,0x0153,0xFFFD,0x017E,0x0178};
static int kindFor(const char *n)
{
  if (!n) return 4;
  if (!strcasecmp(n, "UTF-16LE") || !strcasecmp(n, "UTF16LE") || !strcasecmp(n, "UTF-16")) return 2;
  if (!strcasecmp(n, "UTF-16BE")) return 3;
  if (!strcasecmp(n, "UTF-8") || !strcasecmp(n, "UTF8")) return 1;
  return 4;
}
U_CAPI UConverter *U_EXPORT2 ucnv_open(const char *name, UErrorCode *err)
{
  if (err && U_FAILURE(*err)) return nullptr;
  UConverter *c = static_cast<UConverter *>(std::malloc(sizeof(UConverter)));
  c->kind = kindFor(name);
  return c;
}
U_CAPI void U_EXPORT2 ucnv_close(UConverter *c) { std::free(c); }
U_CAPI UChar32 U_EXPORT2 ucnv_getNextUChar(UConverter *c, const char **source, const char *limit, UErrorCode *err)
{
  const unsigned char *s = reinterpret_cast<const unsigned char *>(*source);
  const unsigned char *e = reinterpret_cast<const unsigned char *>(limit);
  if (s >= e) { *err = U_INDEX_OUTOFBOUNDS_ERROR; return 0xFFFF; }
  UChar32 cp = 0xFFFD;
  if (c->kind == 2 || c->kind == 3)
  {
    if (e - s < 2) { *source = limit; *err = U_TRUNCATED_CHAR_FOUND; return 0xFFFD; }
    unsigned u = c->kind == 2 ? (s[0] | (s[1] << 8)) : ((s[0] << 8) | s[1]);
    s += 2;
    cp = u;
    if (u >= 0xD800 && u <= 0xDBFF && e - s >= 2)
    {
      unsigned u2 = c->kind == 2 ? (s[0] | (s[1] << 8)) : ((s[0] << 8) | s[1]);
      if (u2 >= 0xDC00 && u2 <= 0xDFFF) { s += 2; cp = 0x10000 + ((u - 0xD800) << 10) + (u2 - 0xDC00); }
    }
  }
  else if (c->kind == 1)
  {
    unsigned b0 = *s++;
    int extra = b0 < 0x80 ? 0 : b0 >= 0xF0 ? 3 : b0 >= 0xE0 ? 2 : b0 >= 0xC0 ? 1 : -1;
    if (extra == 0) cp = b0;
    else if (extra > 0 && e - s >= extra) { cp = b0 & (0x3F >> extra); for (int k = 0; k < extra; ++k) cp = (cp << 6) | (*s++ & 0x3F); }
  }
  else
  {
    unsigned b = *s++;
    cp = (b >= 0x80 && b <= 0x9F) ? kCp1252High[b - 0x80] : b;
  }
  *source = reinterpret_cast<const char *>(s);
  return cp;
}
U_CAPI int32_t U_EXPORT2 ucnv_toUChars(UConverter *cnv, UChar *dest, int32_t cap, const char *src, int32_t len, UErrorCode *err)
{
  if (len < 0) len = static_cast<int32_t>(std::strlen(src));
  const char *p = src, *lim = src + len;
  int32_t n = 0;
  while (p < lim)
  {
    UErrorCode e2 = U_ZERO_ERROR;
    UChar32 ch = ucnv_getNextUChar(cnv, &p, lim, &e2);
    if (U_FAILURE(e2)) break;
    if (ch > 0xFFFF) { if (n + 2 <= cap) { dest[n] = (UChar)(0xD7C0 + (ch >> 10)); dest[n + 1] = (UChar)(0xDC00 | (ch & 0x3FF)); } n += 2; }
    else { if (n < cap) dest[n] = (UChar)ch; ++n; }
  }
  if (n < cap) dest[n] = 0;
  (void)err;
  return n;
}
static UCharsetMatch gMatch = { "windows-1252", 50 };
static const UCharsetMatch *gMatches[1] = { &gMatch };
U_CAPI UCharsetDetector *U_EXPORT2 ucsdet_open(UErrorCode *) { return static_cast<UCharsetDetector *>(std::malloc(sizeof(UCharsetDetector))); }
U_CAPI void U_EXPORT2 ucsdet_close(UCharsetDetector *d) { std::free(d); }
U_CAPI void U_EXPORT2 ucsdet_setText(UCharsetDetector *, const char *, int32_t, UErrorCode *) {}
U_CAPI const UCharsetMatch *U_EXPORT2 ucsdet_detect(UCharsetDetector *, UErrorCode *) { return &gMatch; }
U_CAPI const UCharsetMatch **U_EXPORT2 ucsdet_detectAll(UCharsetDetector *, int32_t *found, UErrorCode *) { *found = 1; return gMatches; }
U_CAPI const char *U_EXPORT2 ucsdet_getName(const UCharsetMatch *m, UErrorCode *) { return m->name; }
U_CAPI int32_t U_EXPORT2 ucsdet_getConfidence(const UCharsetMatch *m, UErrorCode *) { return m->confidence; }
U_CAPI const char *U_EXPORT2 ucsdet_getLanguage(const UCharsetMatch *, UErrorCode *) { return ""; }
// Locale helpers libmspub uses to map Windows language IDs (LCIDs) to language and country.
// They only affect language metadata on text, never layout; unknown IDs are reported as errors.
#include <unicode/uloc.h>
struct LcidEntry { uint32_t lcid; const char *tag; };
static const LcidEntry kLcids[] = {
  {0x0401,"ar_SA"},{0x0402,"bg_BG"},{0x0403,"ca_ES"},{0x0404,"zh_Hant_TW"},{0x0405,"cs_CZ"},{0x0406,"da_DK"},
  {0x0407,"de_DE"},{0x0408,"el_GR"},{0x0409,"en_US"},{0x040A,"es_ES"},{0x040B,"fi_FI"},{0x040C,"fr_FR"},
  {0x040E,"hu_HU"},{0x040F,"is_IS"},{0x0410,"it_IT"},{0x0411,"ja_JP"},{0x0412,"ko_KR"},{0x0413,"nl_NL"},
  {0x0414,"nb_NO"},{0x0415,"pl_PL"},{0x0416,"pt_BR"},{0x0418,"ro_RO"},{0x0419,"ru_RU"},{0x041A,"hr_HR"},
  {0x041B,"sk_SK"},{0x041D,"sv_SE"},{0x041E,"th_TH"},{0x041F,"tr_TR"},{0x0421,"id_ID"},{0x0422,"uk_UA"},
  {0x0424,"sl_SI"},{0x0425,"et_EE"},{0x0426,"lv_LV"},{0x0427,"lt_LT"},{0x042A,"vi_VN"},{0x0804,"zh_Hans_CN"},
  {0x0807,"de_CH"},{0x0809,"en_GB"},{0x080A,"es_MX"},{0x080C,"fr_BE"},{0x0813,"nl_BE"},{0x0816,"pt_PT"},
  {0x0C07,"de_AT"},{0x0C09,"en_AU"},{0x0C0A,"es_ES"},{0x0C0C,"fr_CA"},{0x1009,"en_CA"},{0x100C,"fr_CH"},
  {0x1409,"en_NZ"},{0x1809,"en_IE"},{0x1C09,"en_ZA"}};
static int32_t copyOut(const char *s, char *dst, int32_t cap, UErrorCode *err)
{
  int32_t n = static_cast<int32_t>(std::strlen(s));
  if (dst && cap > 0) { int32_t k = n < cap ? n : cap - 1; std::memcpy(dst, s, k); dst[k] = 0; }
  if (n >= cap && err) *err = U_BUFFER_OVERFLOW_ERROR;
  return n;
}
static void splitTag(const char *id, char *lang, char *script, char *country)
{
  lang[0] = script[0] = country[0] = 0;
  if (!id) return;
  const char *p = id; int i = 0;
  while (*p && *p != '_' && *p != '-' && i < 11) lang[i++] = *p++;
  lang[i] = 0;
  while (*p == '_' || *p == '-')
  {
    ++p; char part[12]; int j = 0;
    while (*p && *p != '_' && *p != '-' && j < 11) part[j++] = *p++;
    part[j] = 0;
    if (j == 4 && !script[0] && !country[0]) std::strcpy(script, part);
    else if ((j == 2 || j == 3) && !country[0]) std::strcpy(country, part);
  }
}
U_CAPI int32_t U_EXPORT2 uloc_getLocaleForLCID(uint32_t id, char *locale, int32_t cap, UErrorCode *err)
{
  for (const LcidEntry &e : kLcids) if (e.lcid == id) return copyOut(e.tag, locale, cap, err);
  if (err) *err = U_ILLEGAL_ARGUMENT_ERROR;
  return 0;
}
U_CAPI int32_t U_EXPORT2 uloc_getLanguage(const char *id, char *out, int32_t cap, UErrorCode *err)
{ char l[12], s[12], c[12]; splitTag(id, l, s, c); return copyOut(l, out, cap, err); }
U_CAPI int32_t U_EXPORT2 uloc_getScript(const char *id, char *out, int32_t cap, UErrorCode *err)
{ char l[12], s[12], c[12]; splitTag(id, l, s, c); return copyOut(s, out, cap, err); }
U_CAPI int32_t U_EXPORT2 uloc_getCountry(const char *id, char *out, int32_t cap, UErrorCode *err)
{ char l[12], s[12], c[12]; splitTag(id, l, s, c); return copyOut(c, out, cap, err); }
