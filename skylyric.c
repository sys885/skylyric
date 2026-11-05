/*
 * skylyric.c - "Sky lyric" (native single-file port of deepseek_html_20261101_3895b2.html)
 * Build: gcc -O2 -mwindows -static skylyric.c -o skylyric.exe -lgdi32 -luser32 -lws2_32 -lshell32 -lm
 */
#ifndef WINVER
#define WINVER 0x0603
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0603
#endif
#define _CRT_SECURE_NO_WARNINGS 1

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
#include <math.h>
#include <ws2tcpip.h>

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

#define LY_MAX   512
#define MAXLINES 4096
#define MAXPARTS 512
#define MAXFRAGS 256
/* scattered side characters spawned per lyric line, split evenly between the two
   columns. The count is adjustable from the menu, so this is the default and the
   array bounds come from FRAG_MAXN instead. */
#define FRAG_PER_LINE 5
#define FRAG_MIN      1
#define FRAG_MAXN     16
/* the built-in default; both halves can be changed from the menu and are
   remembered in skylyric.ini beside the executable (see the address section) */
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 23330
#define STATUS_PATH  "/status?filter=status,lyricLineText,progress"
#define LYRIC_PATH   "/lyric"

/* ------------------------------------------------------------------ GDI+ */
typedef void GpGraphics, GpImage, GpBitmap, GpFont, GpFontFamily, GpBrush;
typedef void GpStringFormat, GpMatrix, GpImageAttributes;
typedef int St;

typedef struct { float X, Y, Width, Height; } RectF;
typedef struct { float m[5][5]; } ColorMatrix;

typedef struct {
    u32 GdiplusVersion;
    void *DebugEventCallback;
    int SuppressBackgroundThread;
    int SuppressExternalCodeManager;
} GpStartupInput;

enum { UnitPixel = 2 };
/* values derived from gdipluspixelformats.h macros */
enum { Format32bppARGB = 0x26200a, Format32bppPARGB = 0xe200b };
enum { TextRenderingHintAntiAlias = 4 };
enum { SmoothingModeAntiAlias = 4 };
enum { StringAlignmentNear = 0, StringAlignmentCenter = 1 };
enum { StringTrimmingNone = 2 };
enum { MatrixOrderAppend = 1 };
enum { WrapModeTile = 0 };
enum { ColorAdjustTypeDefault = 0 };

static St    (*G_Startup)(ULONG_PTR *, const GpStartupInput *, void *);
static void  (*G_Shutdown)(ULONG_PTR);
static St    (*G_CreateFromImage)(GpImage *, GpGraphics **);
static void  (*G_DeleteGraphics)(GpGraphics *);
static St    (*G_Flush)(GpGraphics *, int);
static St    (*G_SetPageUnit)(GpGraphics *, int);
static St    (*G_SetTextRenderingHint)(GpGraphics *, int);
static St    (*G_SetSmoothingMode)(GpGraphics *, int);
static St    (*G_CreateBitmapFromScan0)(int, int, int, u32, void *, GpBitmap **);
static void  (*G_DisposeImage)(GpImage *);
static St    (*G_CreateFontFamilyFromName)(const WCHAR *, void *, GpFontFamily **);
static St    (*G_CreateFont)(GpFontFamily *, float, int, int, GpFont **);
static void  (*G_DeleteFont)(GpFont *);
static St    (*G_MeasureString)(GpGraphics *, const WCHAR *, int, GpFont *, RectF *,
                                GpStringFormat *, RectF *, int *, int *);
static St    (*G_DrawString)(GpGraphics *, const WCHAR *, int, GpFont *, RectF *,
                             GpStringFormat *, GpBrush *);
static St    (*G_CreateStringFormat)(int, int, GpStringFormat **);
static St    (*G_StringFormatSetAlignment)(GpStringFormat *, int);
static St    (*G_StringFormatSetTrimming)(GpStringFormat *, int);
static void  (*G_DeleteStringFormat)(GpStringFormat *);
static St    (*G_CreateSolidFill)(u32, GpBrush **);
static void  (*G_DeleteBrush)(GpBrush *);
static HMODULE g_gdMod;
static ULONG_PTR g_gdToken;
static char g_gdFail[64] = "";

#define GLOAD(field, sym)                                     \
    do {                                                      \
        *(void **)(&(field)) = (void *)GetProcAddress(g_gdMod, sym); \
        if (!(field)) { snprintf(g_gdFail, sizeof g_gdFail, "%s", sym); return -1; } \
    } while (0)

static int gdip_load(void)
{
    g_gdMod = LoadLibraryW(L"gdiplus.dll");
    if (!g_gdMod) { snprintf(g_gdFail, sizeof g_gdFail, "%s", "LoadLibrary gdiplus.dll"); return -1; }
    GLOAD(G_Startup, "GdiplusStartup");
    GLOAD(G_Shutdown, "GdiplusShutdown");
    GLOAD(G_CreateFromImage, "GdipGetImageGraphicsContext");
    GLOAD(G_DeleteGraphics, "GdipDeleteGraphics");
    GLOAD(G_Flush, "GdipFlush");
    GLOAD(G_SetPageUnit, "GdipSetPageUnit");
    GLOAD(G_SetTextRenderingHint, "GdipSetTextRenderingHint");
    GLOAD(G_SetSmoothingMode, "GdipSetSmoothingMode");
    GLOAD(G_CreateBitmapFromScan0, "GdipCreateBitmapFromScan0");
    GLOAD(G_DisposeImage, "GdipDisposeImage");
    GLOAD(G_CreateFontFamilyFromName, "GdipCreateFontFamilyFromName");
    GLOAD(G_CreateFont, "GdipCreateFont");
    GLOAD(G_DeleteFont, "GdipDeleteFont");
    GLOAD(G_MeasureString, "GdipMeasureString");
    GLOAD(G_DrawString, "GdipDrawString");
    GLOAD(G_CreateStringFormat, "GdipCreateStringFormat");
    GLOAD(G_StringFormatSetAlignment, "GdipSetStringFormatAlign");
    GLOAD(G_StringFormatSetTrimming, "GdipSetStringFormatTrimming");
    GLOAD(G_DeleteStringFormat, "GdipDeleteStringFormat");
    GLOAD(G_CreateSolidFill, "GdipCreateSolidFill");
    GLOAD(G_DeleteBrush, "GdipDeleteBrush");
    return 0;
}

/* ------------------------------------------------------------- utilities */
static double now_ms(void)
{
    static LARGE_INTEGER fq, t0;
    static int init;
    LARGE_INTEGER n;
    if (!init) { QueryPerformanceFrequency(&fq); QueryPerformanceCounter(&t0); init = 1; }
    QueryPerformanceCounter(&n);
    return (double)(n.QuadPart - t0.QuadPart) * 1000.0 / (double)fq.QuadPart;
}

static double rnd(double lo, double hi) { return lo + (hi - lo) * ((double)rand() / (double)RAND_MAX); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* The default system timer tick is ~15.6ms, which makes Sleep() jitter the frame
   pacing. Ask winmm for 1ms resolution; harmless if it is unavailable. */
static void timer_granularity_begin(void)
{
    HMODULE m = LoadLibraryW(L"winmm.dll");
    if (m) {
        typedef UINT(WINAPI *tbp)(UINT);
        tbp f = (tbp)(void *)GetProcAddress(m, "timeBeginPeriod");
        if (f) f(1);
    }
}

/* cubic-bezier easing, as in CSS timing functions */
static double bez(double x1, double y1, double x2, double y2, double x)
{
    double t, fx, d;
    int i;
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    t = x;
    for (i = 0; i < 12; i++) {
        double u = 1 - t;
        fx = 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t;
        d = 3 * u * u * x1 + 6 * u * t * (x2 - x1) + 3 * t * t * (1 - x2);
        if (fabs(d) < 1e-9) break;
        t -= (fx - x) / d;
        t = clampd(t, 0.0, 1.0);
    }
    {
        double u = 1 - t;
        return 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t;
    }
}
static double ease_linear(double p) { return clampd(p, 0, 1); }
static double ease_in(double p) { return bez(0.42, 0, 1, 1, p); }
static double ease_out(double p) { return bez(0, 0, 0.58, 1, p); }
static double ease_inout(double p) { return bez(0.42, 0, 0.58, 1, p); }

/* rainbow gradient of the CSS linear-gradient(90deg, ...) */
static const struct { double p; u32 rgb; } STOPS[] = {
    {  0.0, 0xff4d6d }, { 20.0, 0xffa94d }, { 38.0, 0xffe066 }, { 55.0, 0x69db7c },
    { 72.0, 0x4dabf7 }, { 88.0, 0x9775fa }, { 100.0, 0xf783ac }
};

static u32 ramp(double pct)
{
    int i;
    double f;
    u32 a, b;
    pct = clampd(pct, 0, 100);
    for (i = 0; i < 6; i++) {
        if (pct <= STOPS[i + 1].p) {
            f = (pct - STOPS[i].p) / (STOPS[i + 1].p - STOPS[i].p);
            a = STOPS[i].rgb; b = STOPS[i + 1].rgb;
            break;
        }
    }
    if (i > 5) { i = 5; f = 1; a = STOPS[5].rgb; b = STOPS[6].rgb; }
    {
        u32 r = (u32)(((a >> 16 & 255) * (1 - f) + (b >> 16 & 255) * f));
        u32 g = (u32)(((a >> 8 & 255) * (1 - f) + (b >> 8 & 255) * f));
        u32 bl = (u32)(((a & 255) * (1 - f) + (b & 255) * f));
        return (r << 16) | (g << 8) | bl;
    }
}

/* utf8 */
static int u8dec(const char *s, u32 *cp)
{
    u8 c = (u8)s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) { *cp = ((c & 31u) << 6) | ((u8)s[1] & 63u); return 2; }
    if ((c & 0xF0) == 0xE0) { *cp = ((c & 15u) << 12) | (((u8)s[1] & 63u) << 6) | ((u8)s[2] & 63u); return 3; }
    if ((c & 0xF8) == 0xF0) { *cp = ((c & 7u) << 18) | (((u8)s[1] & 63u) << 12) | (((u8)s[2] & 63u) << 6) | ((u8)s[3] & 63u); return 4; }
    *cp = c; return 1;
}
static int u8enc(u32 cp, char *out)
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 63)); out[2] = (char)(0x80 | (cp & 63)); return 3; }
    out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 63));
    out[2] = (char)(0x80 | ((cp >> 6) & 63)); out[3] = (char)(0x80 | (cp & 63)); return 4;
}
static int u8ncpy(char *dst, const char *src, int cap)
{
    int n = (int)strlen(src);
    if (n > cap - 1) n = cap - 1;
    memcpy(dst, src, n); dst[n] = 0; return n;
}
static int is_u8_space(u32 cp) { return cp == ' ' || cp == 0x3000 || cp == '\t'; }
static int is_u8_ascii_space(u32 cp) { return cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n'; }
static int is_word_ch(u32 cp)
{
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ||
           cp == '_' || cp == '\'';
}
static int is_cjk_punct(u32 cp)
{
    static const u32 t[] = { 0x2C, 0x3002, 0xFF0C, 0xFF01, 0xFF1F, 0x3001, 0xFF1B, 0xFF1A,
                             0x201C, 0x201D, 0x2018, 0x2019, 0xFF08, 0xFF09, 0x300A, 0x300B,
                             0x300C, 0x300D, 0x2014, 0x2026, 0xFF0E,
                             '.', ',', '!', '?', ':', ';', '"', '\'' };
    size_t i;
    if (is_u8_ascii_space(cp)) return 1;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) if (t[i] == cp) return 1;
    return 0;
}

/* --------------------------------------------------------------- http */
static int http_get(const char *url, char *out, int cap, int timeoutMs)
{
    char host[128], path[1024];
    int port = 80, got = 0, n;
    char req[1600];
    char *hdrEnd, *body;
    u_long mode;
    struct sockaddr_in sa;
    SOCKET s;
    WSADATA wsa;
    DWORD rcvto = timeoutMs;

    path[0] = 0;
    n = sscanf(url, "http://%127[^:/]:%d%1023s", host, &port, path);
    if (n < 2) return -1;
    if (path[0] == 0) strcpy(path, "/");

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return -1; }
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&rcvto, sizeof rcvto);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&rcvto, sizeof rcvto);
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u16)port);
    if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) {
        struct addrinfo *ai = NULL, hints;
        char pbuf[8];
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        _itoa(port, pbuf, 10);
        if (getaddrinfo(host, pbuf, &hints, &ai) != 0 || !ai) {
            closesocket(s); WSACleanup(); return -1;
        }
        memcpy(&sa.sin_addr, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, 4);
        freeaddrinfo(ai);
    }
    if (connect(s, (struct sockaddr *)&sa, sizeof sa) != 0) {
        closesocket(s); WSACleanup(); return -1;
    }
    sprintf(req, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n"
                 "Accept: */*\r\nUser-Agent: skylyric/1.0\r\n\r\n", path, host, port);
    if (send(s, req, (int)strlen(req), 0) <= 0) { closesocket(s); WSACleanup(); return -1; }

    while (got < cap - 1) {
        n = recv(s, out + got, cap - 1 - got, 0);
        if (n <= 0) break;
        got += n;
        out[got] = 0;
        if (strstr(out, "\r\n\r\n")) break;
    }
    if (got < cap - 1) {
        /* read the rest (server closes) */
        while (got < cap - 1) {
            n = recv(s, out + got, cap - 1 - got, 0);
            if (n <= 0) break;
            got += n;
        }
        out[got] = 0;
    }
    closesocket(s);
    WSACleanup();
    if (got <= 4) return -1;

    mode = 0; (void)mode;
    hdrEnd = strstr(out, "\r\n\r\n");
    if (!hdrEnd) return -1;
    body = hdrEnd + 4;
    {
        int hdrLen = (int)(body - out);
        int chunked = 0, keep = got - hdrLen;
        char *p;
        /* scan header block for transfer-encoding */
        {
            int i;
            for (i = 0; i + 25 < hdrLen; i++) {
                p = out + i;
                if (_memicmp(p, "transfer-encoding", 17) == 0 &&
                    _memicmp(p + 17, ": chunked", 9) == 0) { chunked = 1; break; }
            }
        }
        if (!chunked) { memmove(out, body, keep); out[keep] = 0; return keep; }
        /* de-chunk */
        {
            int total = 0;
            char *src = body;
            char *end = out + got;
            for (;;) {
                long sz;
                char *nl;
                if (src >= end) break;
                nl = strstr(src, "\r\n");
                if (!nl) break;
                sz = strtol(src, NULL, 16);
                if (sz <= 0) break;
                src = nl + 2;
                if (src + sz > end) sz = (long)(end - src);
                if (total + (int)sz > cap - 1) sz = cap - 1 - total;
                memcpy(out + total, src, (size_t)sz);
                total += (int)sz;
                src += sz + 2;
                if (total >= cap - 1) break;
            }
            out[total] = 0;
            return total;
        }
    }
}

/* ---------------------------------------------------------------- json */
static const char *jval(const char *j, const char *key)
{
    char pat[80];
    const char *p = j;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    while ((p = strstr(p, pat)) != NULL) {
        const char *q = p + strlen(pat);
        p = q;
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
        if (*q == ':') return q + 1;
    }
    return NULL;
}
static void utf8_put(u32 cp, char *out, int *pos, int cap)
{
    char b[4];
    int n = u8enc(cp, b);
    if (*pos + n >= cap - 1) return;
    memcpy(out + *pos, b, n);
    *pos += n;
    out[*pos] = 0;
}
static int jstr(const char *j, const char *key, char *out, int cap)
{
    const char *v = jval(j, key);
    int pos = 0;
    /* a server that pretty-prints puts whitespace between the colon and the
       opening quote; skipping it costs nothing and widens what we can read */
    while (v && (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')) v++;
    if (!v || *v != '"') { out[0] = 0; return 0; }
    v++;
    while (*v && *v != '"') {
        if (*v == '\\') {
            v++;
            switch (*v) {
            case 'n': utf8_put('\n', out, &pos, cap); break;
            case 't': utf8_put('\t', out, &pos, cap); break;
            case 'r': break;
            case 'b': break;
            case 'f': break;
            case '"': utf8_put('"', out, &pos, cap); break;
            case '\\': utf8_put('\\', out, &pos, cap); break;
            case '/': utf8_put('/', out, &pos, cap); break;
            case 'u': {
                u32 c = 0; int k;
                for (k = 1; k <= 4; k++) {
                    char h = v[k];
                    int d = (h >= '0' && h <= '9') ? h - '0'
                          : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                          : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : 0;
                    c = c * 16 + (u32)d;
                }
                if (c >= 0xD800 && c <= 0xDBFF && v[5] == '\\' && v[6] == 'u') {
                    u32 c2 = 0;
                    for (k = 7; k <= 10; k++) {
                        char h = v[k];
                        int d = (h >= '0' && h <= '9') ? h - '0'
                              : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                              : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : 0;
                        c2 = c2 * 16 + (u32)d;
                    }
                    c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00);
                    v += 6;
                }
                utf8_put(c, out, &pos, cap);
                v += 4;
                break;
            }
            default: utf8_put((u32)(u8)*v, out, &pos, cap); break;
            }
            v++;
        } else {
            u32 c; int adv = u8dec(v, &c);
            utf8_put(c, out, &pos, cap);
            v += adv;
        }
    }
    out[pos] = 0;
    return pos;
}
static int jnum(const char *j, const char *key, double *val)
{
    const char *v = jval(j, key);
    char *end;
    if (!v) return 0;
    *val = strtod(v, &end);
    return end != v;
}

/* ----------------------------------------------------------------- lrc */
typedef struct { long long time; char text[LY_MAX]; } LyricLine;

static void trim(char *s)
{
    int n = (int)strlen(s), i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
    if (i) memmove(s, s + i, n - i + 1);
    n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}

static int parse_lrc(const char *txt, LyricLine *lines, int maxn)
{
    int cnt = 0;
    const char *p = txt;
    while (*p && cnt < maxn) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        char line[4096];
        if (len >= (int)sizeof line) len = (int)sizeof line - 1;
        memcpy(line, p, len); line[len] = 0;
        p = nl ? nl + 1 : p + len;
        if (line[0] == '[') {
            char *mm = line + 1, *ss, *dot, *close;
            int m, s, ms;
            while (*mm >= '0' && *mm <= '9') mm++;
            if (*mm != ':') continue;
            m = atoi(line + 1);
            ss = mm + 1;
            while (*ss >= '0' && *ss <= '9') ss++;
            if (*ss != '.') continue;
            s = atoi(mm + 1);
            dot = ss + 1;
            ms = atoi(dot);
            if (strchr(dot, ':') || strchr(dot, '[')) continue;
            close = strchr(line, ']');
            if (!close) continue;
            {
                char t[LY_MAX];
                u8ncpy(t, close + 1, sizeof t);
                trim(t);
                if (t[0]) {
                    /* tenths or hundredths of a second */
                    int digits = (int)strlen(dot);
                    long long tt = (long long)m * 60000 + (long long)s * 1000 + ms;
                    if (digits == 2) tt = (long long)m * 60000 + (long long)s * 1000 + ms * 10;
                    else if (digits >= 3) tt = (long long)m * 60000 + (long long)s * 1000 + ms / (digits == 3 ? 10 : 100);
                    lines[cnt].time = tt;
                    u8ncpy(lines[cnt].text, t, LY_MAX);
                    cnt++;
                }
            }
        }
    }
    /* sort by time */
    {
        int i, j;
        for (i = 1; i < cnt; i++) {
            LyricLine k = lines[i];
            for (j = i - 1; j >= 0 && lines[j].time > k.time; j--) lines[j + 1] = lines[j];
            lines[j + 1] = k;
        }
    }
    return cnt;
}

/* ------------------------------------------------------ glyph rendering */
typedef struct { u64 key; GpBitmap *bmp; u32 *buf; int w, h, pad; u8 ref; } Glyph;
typedef struct { u64 key; float adv, boxH, ox, oy; int px; } Metric;
typedef struct { int px; GpFont *f; } FontEnt;

#define GCAP 16384
#define MCAP 8192

static Glyph g_gc[GCAP];
static Metric g_mc[MCAP];
static FontEnt g_fonts[512];
static int g_nfonts;
static GpFontFamily *g_family;
static GpStringFormat *g_sf;
static GpGraphics *g_meas;      /* scratch graphics used for measure/draw */

/*
 * The glyph cache is the one thing that must never grow without limit: every
 * entry owns a w*h*4 bitmap, and the animated side fragments mint a new entry
 * for every (character, pixel size, blur bucket) they pass through. Left alone
 * that is hundreds of megabytes after a long listening session, which on a
 * 2-4 GB tablet ends in a failed allocation and a dead process. So the cache
 * carries a byte budget and second-chance (CLOCK) eviction on top of it.
 */
#define GLYPH_BUDGET ((size_t)48 << 20)
static size_t g_cacheBytes;
static unsigned g_cacheCount;
static u32 g_hand;

static void glyph_drop(Glyph *g)
{
    g_cacheBytes -= (size_t)g->w * g->h * 4;
    g_cacheCount--;
    if (g->bmp) G_DisposeImage((GpImage *)g->bmp);
    if (g->buf) free(g->buf);
    memset(g, 0, sizeof *g);
}

/* drop one victim round the clock hand, ignoring the budget; 0 when empty */
static int glyph_evict_one(void)
{
    u32 i;
    for (i = 0; i < GCAP; i++) {
        Glyph *g = &g_gc[g_hand];
        g_hand = (g_hand + 1) % GCAP;
        if (g->key) { glyph_drop(g); return 1; }
    }
    return 0;
}

/* Free room for `need` bytes. One pass clears the reference bits; if the whole
   table was in use a second pass takes a victim regardless, so progress is
   guaranteed and the budget is a real ceiling rather than a hint. */
static void glyph_make_room(size_t need)
{
    u32 start = g_hand, i;
    if (g_cacheBytes + need <= GLYPH_BUDGET) return;
    for (i = 0; i < GCAP; i++) {
        Glyph *g = &g_gc[start + i < GCAP ? start + i : start + i - GCAP];
        if (!g->key) continue;
        if (!g->ref) {
            g_hand = (start + i + 1) % GCAP;
            glyph_drop(g);
            return;
        }
        g->ref = 0;
    }
    glyph_evict_one();
}

static u64 mix_hash(u64 k)
{
    k ^= k >> 33; k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static GpFont *font_of(int px)
{
    int i;
    GpFont *f = NULL;
    for (i = 0; i < g_nfonts; i++) if (g_fonts[i].px == px) return g_fonts[i].f;
    if (g_nfonts >= 512) {
        /* Table full: recycle the oldest entry rather than fail. Sizes are
           quantised upstream, so this is a safety net, not the normal path. */
        G_DeleteFont(g_fonts[0].f);
        memmove(&g_fonts[0], &g_fonts[1], (size_t)(511) * sizeof g_fonts[0]);
        g_nfonts = 511;
    }
    if (G_CreateFont(g_family, (float)px, 0 /*regular*/, UnitPixel, &f) != 0 || !f)
        return NULL;
    g_fonts[g_nfonts].px = px;
    g_fonts[g_nfonts].f = f;
    g_nfonts++;
    return f;
}

static void cp_to_wav(u32 cp, WCHAR *w)
{
    if (cp < 0x10000) { w[0] = (WCHAR)cp; w[1] = 0; }
    else {
        u32 v = cp - 0x10000;
        w[0] = (WCHAR)(0xD800 + (v >> 10));
        w[1] = (WCHAR)(0xDC00 + (v & 0x3FF));
        w[2] = 0;
    }
}

static Metric *metric_of(u32 cp, int px)
{
    u64 key = ((u64)cp << 12) | (u64)(px & 0xFFF);
    u32 idx = (u32)(mix_hash(key) % MCAP);
    static Metric fallback;         /* the one caller-visible place where a
                                       lookup is allowed to miss */
    int probe;
    WCHAR wv[4];
    GpFont *f = font_of(px);
    RectF lay, bb;
    Metric *m = NULL;

    fallback.key = key;
    fallback.px = px;
    fallback.adv = (float)px;
    fallback.boxH = (float)px * 1.3f;
    fallback.ox = 0; fallback.oy = 0;

    if (!f) return &fallback;
    for (probe = 0; probe < 24; probe++) {
        m = &g_mc[(idx + probe) % MCAP];
        if (m->key == key && m->px == px) return m;
        if (m->key == 0) break;
        if (probe == 23) m = NULL;      /* crowded: measure without caching */
    }
    if (!m) m = &fallback;              /* transient slot, never stored */
    {
        int len;
        static Metric scratch;
        Metric *out = (m == &fallback) ? &scratch : m;
        cp_to_wav(cp, wv);
        len = (int)wcslen(wv);
        lay.X = 0; lay.Y = 0; lay.Width = 4000; lay.Height = 4000;
        memset(&bb, 0, sizeof bb);
        if (G_MeasureString(g_meas, wv, len, f, &lay, g_sf, &bb, NULL, NULL) != 0)
            return &fallback;
        out->key = key; out->px = px;
        out->adv = bb.Width > 0.5f ? bb.Width : 0.5f;
        out->boxH = bb.Height > 1.0f ? bb.Height : (float)px * 1.3f;
        out->ox = bb.X; out->oy = bb.Y;
        return out;
    }
}

/*
 * Separable box blur on premultiplied ARGB, 3 passes ~= gaussian.
 * Running-sum windows make this O(pixels) instead of O(pixels * radius), and the
 * divide-by-window becomes a reciprocal multiply -- both matter a lot on an Atom.
 * Edges are replicated, which is visually indistinguishable from the clamped window.
 */
static u32 *g_blurTmp;
static size_t g_blurTmpCap;

static void blur_rows(const u32 *src, u32 *dst, int w, int h, int r, unsigned long long recip)
{
    int y, x, k;
    for (y = 0; y < h; y++) {
        const u32 *srow = src + (size_t)y * w;
        u32 *drow = dst + (size_t)y * w;
        unsigned long long a0 = 0, a1 = 0, a2 = 0, a3 = 0;
        for (k = -r; k <= r; k++) {
            u32 v = srow[k < 0 ? 0 : (k >= w ? w - 1 : k)];
            a0 += (v >> 24) & 255; a1 += (v >> 16) & 255;
            a2 += (v >> 8) & 255;  a3 += v & 255;
        }
        for (x = 0; x < w; x++) {
            drow[x] = (u32)(((a0 * recip) >> 32) << 24) |
                      (u32)(((a1 * recip) >> 32) << 16) |
                      (u32)(((a2 * recip) >> 32) << 8)  |
                           ((a3 * recip) >> 32);
            if (x + 1 < w) {
                u32 add = srow[x + r + 1 >= w ? w - 1 : x + r + 1];
                u32 sub = srow[x - r < 0 ? 0 : x - r];
                /* the differences must be signed: both operands are u32, so
                   (small - large) would wrap to ~4e9 and poison the sum */
                a0 += (unsigned long long)((int)((add >> 24) & 255) -
                                           (int)((sub >> 24) & 255));
                a1 += (unsigned long long)((int)((add >> 16) & 255) -
                                           (int)((sub >> 16) & 255));
                a2 += (unsigned long long)((int)((add >> 8) & 255) -
                                           (int)((sub >> 8) & 255));
                a3 += (unsigned long long)((int)(add & 255) - (int)(sub & 255));
            }
        }
    }
}

static void blur_cols(const u32 *src, u32 *dst, int w, int h, int r, unsigned long long recip)
{
    unsigned long long *s0, *s1, *s2, *s3;
    int y, x, k;
    size_t bytes = (size_t)w * sizeof(unsigned long long);
    s0 = (unsigned long long *)malloc(bytes * 4);
    if (!s0) return;
    s1 = s0 + w; s2 = s1 + w; s3 = s2 + w;
    memset(s0, 0, bytes * 4);
    for (k = -r; k <= r; k++) {
        const u32 *row = src + (size_t)(k < 0 ? 0 : (k >= h ? h - 1 : k)) * w;
        for (x = 0; x < w; x++) {
            u32 v = row[x];
            s0[x] += (v >> 24) & 255; s1[x] += (v >> 16) & 255;
            s2[x] += (v >> 8) & 255;  s3[x] += v & 255;
        }
    }
    for (y = 0; y < h; y++) {
        u32 *drow = dst + (size_t)y * w;
        for (x = 0; x < w; x++) {
            drow[x] = (u32)(((s0[x] * recip) >> 32) << 24) |
                      (u32)(((s1[x] * recip) >> 32) << 16) |
                      (u32)(((s2[x] * recip) >> 32) << 8)  |
                           ((s3[x] * recip) >> 32);
        }
        if (y + 1 < h) {
            const u32 *add = src + (size_t)(y + r + 1 >= h ? h - 1 : y + r + 1) * w;
            const u32 *sub = src + (size_t)(y - r < 0 ? 0 : y - r) * w;
            for (x = 0; x < w; x++) {
                u32 av = add[x], sv = sub[x];
                s0[x] += (unsigned long long)((int)((av >> 24) & 255) -
                                              (int)((sv >> 24) & 255));
                s1[x] += (unsigned long long)((int)((av >> 16) & 255) -
                                              (int)((sv >> 16) & 255));
                s2[x] += (unsigned long long)((int)((av >> 8) & 255) -
                                              (int)((sv >> 8) & 255));
                s3[x] += (unsigned long long)((int)(av & 255) - (int)(sv & 255));
            }
        }
    }
    free(s0);
}

static void blur_argb(u32 *img, int w, int h, int r)
{
    unsigned long long recip;
    int win, pass;
    u32 *tmp;
    size_t need = (size_t)w * h * 4;
    if (r <= 0) return;
    win = 2 * r + 1;
    recip = ((1ULL << 32) + (unsigned long long)win - 1) / (unsigned long long)win;
    if (g_blurTmpCap < need) {
        free(g_blurTmp);
        g_blurTmp = (u32 *)malloc(need * 2);
        g_blurTmpCap = g_blurTmp ? need * 2 : 0;
        if (!g_blurTmp) return;
    }
    tmp = g_blurTmp;
    for (pass = 0; pass < 3; pass++) {
        blur_rows(img, tmp, w, h, r, recip);
        blur_cols(tmp, img, w, h, r, recip);
    }
}

/*
 * Cached glyphs are stored as plain white, anti-aliased, premultiplied coverage.
 * Colour is applied at draw time through a ColorMatrix tint, which is a linear
 * per-channel scale and therefore commutes with the blur. Keeping colour out of
 * the cache key is what makes the animation cheap: the old key mixed in the
 * glyph's gradient position, which changes every frame, so almost nothing hit.
 *
 * grad=1 is the one exception: the explosion particles need the rainbow swept
 * across each glyph, which a constant tint cannot express, so it is baked in.
 */
static u32 g_rampLut[256];

static void ramp_lut_init(void)
{
    int i;
    for (i = 0; i < 256; i++) g_rampLut[i] = ramp((double)i / 255.0 * 100.0);
}

static const u32 SOLIDS[] = { 0x4b0082 /* pending */, 0x8ab4ff /* side left */,
                              0xff9ecb /* side right */, 0xffffff /* white */ };
#define SOLID_PENDING 0
#define SOLID_LEFT    1
#define SOLID_RIGHT   2
#define SOLID_WHITE   3

static u64 glyph_key(u32 cp, int px, int blurR, int glow, int grad)
{
    u64 k = 0;
    k |= (cp & 0x1FFFFFu);
    k |= (u64)(px & 0x3FF) << 21;
    k |= (u64)(blurR & 0xF) << 31;
    k |= (u64)(glow & 1) << 35;
    k |= (u64)(grad & 1) << 36;
    return k;
}

static Glyph *glyph_of(u32 cp, int px, int blurR, int glow, int grad)
{
    u64 key = glyph_key(cp, px, blurR, glow, grad);
    u32 idx = (u32)(mix_hash(key) % GCAP);
    int probe;
    Metric *m;
    GpFont *f;
    WCHAR wv[4];
    int len, w, h, pad;
    u32 *buf;
    GpBitmap *bmp = NULL;
    GpGraphics *gg = NULL;
    GpBrush *br = NULL;
    RectF lay;
    Glyph *slot = &g_gc[idx];

    for (probe = 0; probe < 40; probe++) {
        Glyph *g = &g_gc[(idx + probe) % GCAP];
        if (g->key == key) { g->ref = 1; return g; }
        if (g->key == 0) { slot = g; break; }
        slot = g; /* last resort: evict the final probed entry */
    }
    /* Falling out of the loop with no free slot is not an error any more: the
       budget sweep runs first and `slot` still names an entry we may take. */

    m = metric_of(cp, px);
    f = font_of(px);
    if (!m || !f) return NULL;

    pad = 4 + 3 * blurR + (glow ? 6 : 0);
    w = (int)(m->adv + 0.999f) + 2 * pad;
    h = (int)(m->boxH + 0.999f) + 2 * pad;
    if (w < 4) w = 4;
    if (h < 4) h = 4;

    /* stay inside the byte budget, then take the slot over */
    glyph_make_room((size_t)w * h * 4);
    if (slot->key) glyph_drop(slot);

    buf = (u32 *)calloc((size_t)w * h, 4);
    if (!buf) {
        int i;
        /* the cache is the biggest thing we hold: shave it and try again */
        for (i = 0; i < 512 && !buf; i++) {
            if (!glyph_evict_one()) break;
            buf = (u32 *)calloc((size_t)w * h, 4);
        }
        if (!buf) return NULL;
    }
    g_cacheBytes += (size_t)w * h * 4;
    g_cacheCount++;

    /* white ink, premultiplied: every pixel ends up as (a,a,a,a) with a = coverage */
    if (G_CreateBitmapFromScan0(w, h, w * 4, Format32bppPARGB, buf, &bmp) != 0 || !bmp) {
        free(buf); return NULL;
    }
    if (G_CreateFromImage((GpImage *)bmp, &gg) != 0 || !gg) {
        G_DisposeImage((GpImage *)bmp); free(buf); return NULL;
    }
    G_SetPageUnit(gg, UnitPixel);
    G_SetTextRenderingHint(gg, TextRenderingHintAntiAlias);
    G_SetSmoothingMode(gg, SmoothingModeAntiAlias);
    if (G_CreateSolidFill(0xFFFFFFFFu, &br) == 0 && br) {
        cp_to_wav(cp, wv);
        len = (int)wcslen(wv);
        lay.X = (float)pad - m->ox;
        lay.Y = (float)pad - m->oy;
        lay.Width = 4000; lay.Height = 4000;
        G_DrawString(gg, wv, len, f, &lay, g_sf, br);
        G_DeleteBrush(br);
    }
    G_Flush(gg, 1);
    G_DeleteGraphics(gg);
    /* The compositor reads the pixels straight out of `buf`, so the GDI+ bitmap
       was only ever a rasterisation view: release it here instead of keeping
       one live image object per cache entry. */
    G_DisposeImage((GpImage *)bmp);
    bmp = NULL;

    if (grad) {
        /* sweep the rainbow across the glyph: one colour per column, so the
           per-pixel work is three multiplies instead of a full ramp() call */
        u32 *col = (u32 *)malloc((size_t)w * 4);
        if (col) {
            int x, y;
            for (x = 0; x < w; x++) col[x] = g_rampLut[w > 1 ? x * 255 / (w - 1) : 0];
            for (y = 0; y < h; y++) {
                u32 *row = buf + (size_t)y * w;
                for (x = 0; x < w; x++) {
                    u32 a = row[x] >> 24;
                    u32 c = col[x];
                    u32 r = (c >> 16) & 255, g2 = (c >> 8) & 255, b2 = c & 255;
                    r = r * a + 128; g2 = g2 * a + 128; b2 = b2 * a + 128;
                    row[x] = (a << 24) | (((r + (r >> 8)) >> 8) << 16) |
                             (((g2 + (g2 >> 8)) >> 8) << 8) | ((b2 + (b2 >> 8)) >> 8);
                }
            }
            free(col);
        }
    }
    if (blurR > 0) blur_argb(buf, w, h, blurR);

    slot->key = key; slot->bmp = NULL; slot->buf = buf;
    slot->w = w; slot->h = h; slot->pad = pad; slot->ref = 1;
    return slot;
}

/* ---------------------------------------------------------------- compositor
 * GDI+ costs ~0.16ms per GdipDrawImageRectRect call on this workload, which adds
 * up to ~11ms/frame for the ~70 glyphs a lyric line can put on screen. Since both
 * the cached glyphs and the back buffer are our own 32bpp premultiplied ARGB, the
 * compositing is done here instead: integer source-over blend, no image attributes.
 * GDI+ is still used to rasterise the glyphs themselves.
 */
static u32 *g_dst;
static int  g_dstW, g_dstH;

#define TINT_R(t) (((t) >> 16) & 255)
#define TINT_G(t) (((t) >> 8) & 255)
#define TINT_B(t) ((t) & 255)

/* axis-aligned copy: the common case (lyric lines, hint text) */
static void comp_glyph_axis(const Glyph *e, int dx, int dy, unsigned A, u32 tint)
{
    int tr = (TINT_R(tint) * 256 + 128) / 255, tg = (TINT_G(tint) * 256 + 128) / 255,
        tb = (TINT_B(tint) * 256 + 128) / 255;
    int x0, y0, x1, y1, rowLen, x, y;

    if (dx >= g_dstW || dy >= g_dstH || dx <= -e->w || dy <= -e->h) return;
    x0 = dx < 0 ? 0 : dx;
    y0 = dy < 0 ? 0 : dy;
    x1 = dx + e->w; if (x1 > g_dstW) x1 = g_dstW;
    y1 = dy + e->h; if (y1 > g_dstH) y1 = g_dstH;
    if (x1 <= x0 || y1 <= y0) return;
    rowLen = x1 - x0;

    for (y = y0; y < y1; y++) {
        const u32 *srow = e->buf + (size_t)(y - dy) * e->w + (x0 - dx);
        u32 *drow = g_dst + (size_t)y * g_dstW + x0;
        for (x = 0; x < rowLen; x++) {
            u32 s = srow[x];
            unsigned sa = s >> 24;
            unsigned ia, r, g2, b2, d;
            if (!sa) continue;
            sa = (sa * A) >> 8;
            if (!sa) continue;
            ia = 255 - sa;
            d = drow[x];
            r  = ((((s >> 16) & 255) * tr) >> 8) * A >> 8;
            g2 = ((((s >> 8) & 255) * tg) >> 8) * A >> 8;
            b2 = (((s & 255) * tb) >> 8) * A >> 8;
            r  += ((d >> 16) & 255) * ia >> 8;
            g2 += ((d >> 8) & 255) * ia >> 8;
            b2 += (d & 255) * ia >> 8;
            drow[x] = 0xFF000000u | (r << 16) | (g2 << 8) | b2;
        }
    }
}

/* rotated / scaled glyph: walk the destination bbox, inverse-map, bilinear sample */
static void comp_glyph_affine(const Glyph *e, float a, float b, float c, float dd,
                              float tx, float ty, unsigned A, u32 tint)
{
    int tr = (TINT_R(tint) * 256 + 128) / 255, tg = (TINT_G(tint) * 256 + 128) / 255,
        tb = (TINT_B(tint) * 256 + 128) / 255;
    float det, ia_, ib_, ic_, id_;
    float x0f, y0f, x1f, y1f;
    int X, Y, X0, Y0, X1, Y1, sw = e->w, sh = e->h;
    const float hw = 0.5f;

    /* corners of the source rect through the forward transform */
    {
        float px[4], py[4];
        int i;
        px[0] = tx;                    py[0] = ty;
        px[1] = a * sw + tx;           py[1] = c * sw + ty;
        px[2] = a * sw + b * sh + tx;  py[2] = c * sw + dd * sh + ty;
        px[3] = b * sh + tx;           py[3] = dd * sh + ty;
        x0f = x1f = px[0];
        y0f = y1f = py[0];
        for (i = 1; i < 4; i++) {
            if (px[i] < x0f) x0f = px[i];
            if (px[i] > x1f) x1f = px[i];
            if (py[i] < y0f) y0f = py[i];
            if (py[i] > y1f) y1f = py[i];
        }
    }
    X0 = (int)floorf(x0f); Y0 = (int)floorf(y0f);
    X1 = (int)ceilf(x1f);  Y1 = (int)ceilf(y1f);
    if (X0 < 0) X0 = 0;
    if (Y0 < 0) Y0 = 0;
    if (X1 > g_dstW) X1 = g_dstW;
    if (Y1 > g_dstH) Y1 = g_dstH;
    if (X1 <= X0 || Y1 <= Y0) return;

    det = a * dd - b * c;
    if (det > -1e-6f && det < 1e-6f) return;
    ia_ =  dd / det; ib_ = -b / det;
    ic_ = -c / det;  id_ =  a / det;

    for (Y = Y0; Y < Y1; Y++) {
        float oy = (float)Y + hw - ty;
        float su = ia_ * ((float)X0 + hw - tx) + ib_ * oy;
        float sv = ic_ * ((float)X0 + hw - tx) + id_ * oy;
        u32 *drow = g_dst + (size_t)Y * g_dstW;
        for (X = X0; X < X1; X++, su += ia_, sv += ic_) {
            int fx = (int)floorf(su), fy = (int)floorf(sv);
            unsigned wx, wy, sa, r, g2, b2, d, ia;
            const u32 *p00, *p01, *p10, *p11;
            u32 s00, s01, s10, s11;
            if (fx < 0 || fy < 0 || fx >= sw || fy >= sh) continue;
            wx = (unsigned)((su - (float)fx) * 256.0f);
            wy = (unsigned)((sv - (float)fy) * 256.0f);
            if (wx > 256) wx = 256;
            if (wy > 256) wy = 256;
            {
                int x1 = fx + 1 >= sw ? sw - 1 : fx + 1;
                int y1 = fy + 1 >= sh ? sh - 1 : fy + 1;
                p00 = e->buf + (size_t)fy * sw + fx;
                p01 = e->buf + (size_t)fy * sw + x1;
                p10 = e->buf + (size_t)y1 * sw + fx;
                p11 = e->buf + (size_t)y1 * sw + x1;
            }
            s00 = *p00; s01 = *p01; s10 = *p10; s11 = *p11;
            if (!((s00 | s01 | s10 | s11) & 0xFF000000u)) continue;   /* all four clear */
            {
                unsigned w00 = (256 - wx) * (256 - wy), w01 = wx * (256 - wy);
                unsigned w10 = (256 - wx) * wy,        w11 = wx * wy;
                unsigned acc[4], src[4];
                src[0] = s00; src[1] = s01; src[2] = s10; src[3] = s11;
                acc[0] = (src[0] >> 24) * w00 + (src[1] >> 24) * w01 +
                         (src[2] >> 24) * w10 + (src[3] >> 24) * w11;
                sa = (unsigned)((acc[0] >> 16) * A >> 8);
                if (!sa) continue;
                acc[1] = ((src[0] >> 16) & 255) * w00 + ((src[1] >> 16) & 255) * w01 +
                         ((src[2] >> 16) & 255) * w10 + ((src[3] >> 16) & 255) * w11;
                acc[2] = ((src[0] >> 8) & 255) * w00 + ((src[1] >> 8) & 255) * w01 +
                         ((src[2] >> 8) & 255) * w10 + ((src[3] >> 8) & 255) * w11;
                acc[3] = (src[0] & 255) * w00 + (src[1] & 255) * w01 +
                         (src[2] & 255) * w10 + (src[3] & 255) * w11;
                r  = (unsigned)((((acc[1] >> 16) * tr) >> 8) * A >> 8);
                g2 = (unsigned)((((acc[2] >> 16) * tg) >> 8) * A >> 8);
                b2 = (unsigned)((((acc[3] >> 16) * tb) >> 8) * A >> 8);
                if (r > sa) r = sa;
                if (g2 > sa) g2 = sa;
                if (b2 > sa) b2 = sa;
            }
            ia = 255 - sa;
            d = drow[X];
            r  += ((d >> 16) & 255) * ia >> 8;
            g2 += ((d >> 8) & 255) * ia >> 8;
            b2 += (d & 255) * ia >> 8;
            drow[X] = 0xFF000000u | (r << 16) | (g2 << 8) | b2;
        }
    }
}

static void clear_backbuffer(void)
{
    size_t n = (size_t)g_dstW * g_dstH, i;
    u32 *p = g_dst;
    for (i = 0; i + 4 <= n; i += 4) { p[i] = 0xFF000000u; p[i+1] = 0xFF000000u;
                                      p[i+2] = 0xFF000000u; p[i+3] = 0xFF000000u; }
    for (; i < n; i++) p[i] = 0xFF000000u;
}


/* blur radius buckets used for cross-fading. Each extra bucket is another full
   bitmap per character in the cache, so they are deliberately sparse; the
   cross-fade between two of them keeps the blur visually continuous. */
static const int BR[4] = { 0, 2, 4, 8 };

/*
 * Set when frames are genuinely overrunning, i.e. on a CPU that cannot afford
 * the full effect. It trades the least-noticeable work (the halo under flying
 * fragments, half of them, and a coarser size ladder) for frame time, so a slow
 * tablet plays a slightly plainer animation smoothly rather than a rich one in
 * jumps. Never set on a machine that is keeping up, so desktops see no change.
 */
static int g_lowQ;

static void blur_pair(float r, int *lo, int *hi, float *wh)
{
    int i;
    if (r <= 0) { *lo = 0; *hi = 0; *wh = 0; return; }
    if (r >= BR[3]) { *lo = BR[3]; *hi = BR[3]; *wh = 0; return; }
    for (i = 0; i < 3; i++) {
        if (r >= BR[i] && r <= BR[i + 1]) {
            *lo = BR[i]; *hi = BR[i + 1];
            if (BR[i + 1] == BR[i]) *wh = 0;
            else *wh = (r - BR[i]) / (float)(BR[i + 1] - BR[i]);
            return;
        }
    }
    *lo = 0; *hi = 0; *wh = 0;
}

typedef struct { u32 cp; float x, adv; } Ch;

static int layout_text(const char *txt, int px, float spacing, int dropSpaces,
                       Ch *out, int maxn, float *totalW, float *boxH)
{
    int n = 0;
    float x = 0;
    const char *p = txt;
    u32 c;
    *totalW = 0; *boxH = (float)px * 1.3f;
    while (*p && n < maxn) {
        Metric *m;
        int adv = u8dec(p, &c);
        p += adv;
        if (dropSpaces && is_u8_space(c)) { x += 10.0f; continue; }
        m = metric_of(c, px);
        if (!m) continue;
        out[n].cp = c; out[n].x = x; out[n].adv = m->adv;
        *boxH = m->boxH;
        n++;
        x += m->adv + spacing;
    }
    *totalW = n ? x - spacing : 0;
    return n;
}

static void put_glyph(const Glyph *e, float ux, float uy, int ident,
                      float ma, float mb, float mc, float md, float mtx, float mty,
                      float a, u32 tint)
{
    unsigned A;
    float tx, ty;
    /* the glyph may be missing (allocation failure); everything downstream
       assumes it exists, so the check has to be here, before e->pad is read */
    if (!e || a <= 0.004f) return;
    A = (unsigned)(a * 256.0f);
    if (A > 256) A = 256;
    if (!A) return;
    ux -= (float)e->pad;
    uy -= (float)e->pad;
    if (ident) {
        comp_glyph_axis(e, (int)(ux + 0.5f), (int)(uy + 0.5f), A, tint);
    } else {
        /* ma..mty is the line's world transform; the glyph sits at ux,uy, so the
           transform of the glyph's own (0,0) is the translate the sampler needs. */
        tx = ma * ux + mb * uy + mtx;
        ty = mc * ux + md * uy + mty;
        comp_glyph_affine(e, ma, mb, mc, md, tx, ty, A, tint);
    }
}

/*
 * Draw a whole text line into the back buffer.
 *   ox, oy : layout origin (top-left of the text box) in world coords
 *   colMode: 0 = ramp at line position, 1 = ramp across each glyph, 2 = solid palette colour
 *   scale / rotDeg : applied about the centre of the line box (CSS transform behaviour)
 */
static void draw_line(const char *txt, int px, float spacing, float ox, float oy,
                      float alpha, int colMode, int solidIdx, float blurR,
                      float glowA, int glowR, float scale, float rotDeg, int dropSpaces)
{
    static Ch chs[256];
    float totalW, boxH;
    int n, i, ident;
    float cx, cy, ma, mb, mc, md, mtx, mty;

    if (!txt || !txt[0] || alpha <= 0.004f || px < 1) return;
    n = layout_text(txt, px, spacing, dropSpaces, chs, 256, &totalW, &boxH);
    if (!n) return;

    cx = ox + totalW / 2.0f;
    cy = oy + boxH / 2.0f;
    if (scale != 1.0f || rotDeg != 0.0f) {
        /* p' = S*R*(p - c) + c, matching the CSS transform-box behaviour */
        float rad = rotDeg * 0.017453292519943295f;
        float co = cosf(rad) * scale, si = sinf(rad) * scale;
        ma = co; mb = -si; mc = si; md = co;
        mtx = cx - (ma * cx + mb * cy);
        mty = cy - (mc * cx + md * cy);
        ident = 0;
    } else {
        ma = md = 1.0f; mb = mc = mtx = mty = 0.0f;
        ident = 1;
    }

    {
        int rlo, rhi;
        float wh;
        int grad = (colMode == 1);
        blur_pair(blurR, &rlo, &rhi, &wh);

        for (i = 0; i < n; i++) {
            float frac = (chs[i].x + chs[i].adv / 2.0f) / (totalW > 0 ? totalW : 1.0f);
            u32 tint = colMode == 2 ? SOLIDS[solidIdx & 3]
                         : colMode == 1 ? 0xFFFFFFu
                         : g_rampLut[(int)clampd(frac * 255.0, 0, 255)];
            float ux = ox + chs[i].x;
            Glyph *e;

            e = glyph_of(chs[i].cp, px, rlo, 0, grad);
            put_glyph(e, ux, oy, ident, ma, mb, mc, md, mtx, mty,
                      alpha * (1.0f - wh), tint);
            if (rhi != rlo && wh > 0.0f) {
                e = glyph_of(chs[i].cp, px, rhi, 0, grad);
                put_glyph(e, ux, oy, ident, ma, mb, mc, md, mtx, mty,
                          alpha * wh, tint);
            }
            if (glowA > 0.004f) {
                /* the glow bitmap is plain white coverage; its old 0.75 dimming
                   factor now rides along in the draw alpha */
                e = glyph_of(chs[i].cp, px, glowR, 1, 0);
                put_glyph(e, ux, oy, ident, ma, mb, mc, md, mtx, mty,
                          glowA * alpha * 0.75f, 0xFFFFFFu);
            }
        }
    }
}

/* line metrics without drawing (for centring) */
static float line_width(const char *txt, int px, float spacing, int dropSpaces, float *boxH)
{
    static Ch chs[256];
    float totalW = 0, bh = (float)px * 1.3f;
    layout_text(txt, px, spacing, dropSpaces, chs, 256, &totalW, &bh);
    if (boxH) *boxH = bh;
    return totalW;
}

/* ------------------------------------------------------------------ state */
/*
 * Display modes, picked from the startup menu (see show_mode_menu) and from the
 * right-click menu afterwards. All four are overlays; only the rectangle they
 * cover differs.
 */
#define MODE_FULL     1   /* whole monitor                        */
#define MODE_QUARTER  2   /* full width, bottom quarter (default) */
#define MODE_EIGHTH   3   /* full width, bottom eighth            */
#define MODE_TASKBAR  4   /* a strip the height of the taskbar    */

static int   g_mode = MODE_QUARTER;
/* Glyph sizes are authored for a 360px band; g_k scales them for the others.
   It is refreshed by backbuffer_reset(), which sees every size change. */
static float g_k = 1.0f;

/* The user's three display knobs, declared here because the size macros below
   read them. They are mirrors of the locked values in the settings section: each
   is a single word, so the compositor can take them without locking and the
   worst a torn read can do is paint one frame at the previous size. */
static float g_fsMainF = 1.0f;    /* middle line, percent of the mode's size */
static float g_fsFragF = 1.0f;    /* scattered side characters, likewise */
static int   g_fragCntN = 5;      /* how many of them one line throws off */

/* layout mirrors the CSS flexbox: [2vw pad][20vw side][2vw gap][52vw stage][2vw gap][20vw side][2vw pad] */
typedef struct {
    float winW, winH, sideX[2], sideW, stageX, stageY, stageW, stageH, fragScale;
    int   showFrags;   /* side fragments need vertical room to drift in */
    int   showPend;    /* the next-line preview needs room under the line */
} Laid;

static Laid layout_of(float W, float H)
{
    Laid L;
    float gap = W * 0.02f, sw = W * 0.20f, stw = W * 0.52f, pad = W * 0.02f;
    L.winW = W; L.winH = H;
    L.sideX[0] = pad;
    L.stageX = pad + sw + gap;
    L.sideX[1] = L.stageX + stw + gap;
    L.sideW = sw;
    L.stageW = stw;
    L.showFrags = 1;
    L.showPend = 1;
    if (g_mode == MODE_TASKBAR) {
        /* One line, no vertical room: the stage fills the strip and the side
           fragments shrink to fit it rather than disappearing with the height.
           The flex split keeps its side columns so the words still drift in from
           both edges. The strip is authored against its own 78px reference (the
           same one the main line uses), so the tiers land at 16 / 29 / 52 px in a
           48px taskbar instead of the unreadable 12px the /360 curve gave. */
        L.stageH = H;
        L.stageY = 0;
        L.showPend = 0;
        L.fragScale = H / 78.0f;
        if (L.fragScale > 0.85f) L.fragScale = 0.85f;
        if (L.fragScale < 0.55f) L.fragScale = 0.55f;
        return L;
    }
    /* .side-region is 200px tall in the CSS and its fragments overflow it by up to
       110% plus an 85px glyph; shrink both the box and the glyphs so that overshoot
       still lands inside the short band instead of being cut off by the window. */
    L.stageH = H * 0.40f;
    L.stageY = (H - L.stageH) / 2.0f;
    /* The 1/8 band is too short for the /360 curve: it would put the main line at
       15px and the drifting words at 12px, which is legible nowhere. It gets its
       own 220px reference, which is still small enough that the line plus the
       next-line preview fit inside the band. */
    if (g_mode == MODE_EIGHTH) {
        L.fragScale = H / 220.0f;
        if (L.fragScale > 0.85f) L.fragScale = 0.85f;
        if (L.fragScale < 0.50f) L.fragScale = 0.50f;
        return L;
    }
    L.fragScale = H / 360.0f;
    if (L.fragScale > 1.0f) L.fragScale = 1.0f;
    if (L.fragScale < 0.45f) L.fragScale = 0.45f;
    return L;
}

/* The two macros with g_fsMainF in them carry the user's size knob: the middle
   line, its preview and their letter spacing all move together, so a line that
   fitted before still looks typeset rather than merely larger. */
#define FONT_MAIN ((int)(42.0f * g_k * g_fsMainF))
#define FONT_PEND ((int)(21.0f * g_k * g_fsMainF))
#define PEND_DY   (72.0f * g_k * g_fsMainF)
/* letter spacing is authored for the 360px band too, so it scales with it */
#define SP(s)     ((float)(s) * g_k * g_fsMainF)

/*
 * A long English line is far wider than a Chinese one at the same point size and
 * used to run off both edges of the monitor. Measure it, and when it is wider
 * than the middle two thirds of the band shrink the glyph size and the letter
 * spacing by the same factor, so the whole line stays on screen and keeps its
 * centred position. The result is cached per (line, size, spacing): measuring
 * costs a GDI+ pass and the same line is measured on every frame.
 */
typedef struct { const char *text; int px; float sp, winW, f; } FitEntry;
static FitEntry g_fit[3];
static int      g_fitNext;

static float line_fit(const Laid *L, const char *text, int px, float sp)
{
    float maxW = L->winW * 0.66667f;
    float f = 1.0f, w;
    int i;
    if (!text || !text[0] || px < 4 || maxW < 40.0f) return 1.0f;
    for (i = 0; i < 3; i++) {
        if (g_fit[i].text == text && g_fit[i].px == px &&
            g_fit[i].sp == sp && g_fit[i].winW == L->winW)
            return g_fit[i].f;
    }
    w = line_width(text, px, sp, 0, NULL);
    if (w > maxW) {
        f = maxW / w;
        w = line_width(text, (int)(px * f + 0.5f), sp * f, 0, NULL);
        if (w > maxW) f *= maxW / w;
        if (f < 0.30f) f = 0.30f;          /* never shrink past a readable floor */
    }
    i = g_fitNext;
    g_fitNext = (g_fitNext + 1) % 3;
    g_fit[i].text = text; g_fit[i].px = px; g_fit[i].sp = sp;
    g_fit[i].winW = L->winW; g_fit[i].f = f;
    return f;
}

/* current line phases, matching the CSS animation sequence */
enum { PH_NONE = 0, PH_FLY = 1, PH_SPREAD = 2, PH_IDLE = 3 };

static char   g_cur[LY_MAX] = "";
static char   g_pend[LY_MAX] = "";
static int    g_phase = PH_NONE;
static double g_phaseT0 = 0;
static double g_pendT0 = 0, g_pendFrom = 0, g_pendTo = 0;
static double g_driftT0 = 0, g_driftAx = 0, g_driftAy = 0;
static int    g_driftOn = 0;
static double g_pendDx = 0, g_pendDy = 0;

/* particles */
typedef struct {
    int used;
    u32 cp;
    float x0, y0, adv, boxH;
    int px;                     /* the size this glyph was laid out at */
    double dx, dy, scT, rotT, t0;
} Part;
static Part g_parts[MAXPARTS];

/* side fragments */
typedef struct {
    int used;
    char txt[128];
    int side, tier;
    double l0, t0pos, l1, t1pos;   /* position in % of the region */
    double sz0, sz1, op0, op1, bl0, bl1, sc0, sc1;
    double tStart;
    double rot;
    double dAx, dAy, dPh;
} Frag;
static Frag g_frags[MAXFRAGS];

/* the side characters carry the user's second size knob; the tiers stay in
   proportion to each other, only the whole ladder moves */
static double tier_size(int tier)
{
    return (tier <= 1 ? 26 : (tier == 2 ? 48 : 85)) * (double)g_fsFragF;
}
static double tier_op(int tier)   { return tier <= 1 ? 0.95 : (tier == 2 ? 0.45 : 0.15); }
static double tier_blur(int tier) { return tier <= 1 ? 0 : (tier == 2 ? 2 : 6); }

/* --------------------------- character explosion --------------------------- */
static void explode_text(const char *text, const Laid *L, double now)
{
    static Ch chs[256];
    float totalW = 0, boxH = 0;
    float startX, startY, sp;
    float f;
    int px, n, i, made = 0;

    if (!text || !text[0]) return;
    /* the debris starts from the same fitted line the stage is showing, so a
       long English verse does not explode from off the edge of the screen */
    f = line_fit(L, text, FONT_MAIN, SP(7));
    px = (int)(FONT_MAIN * f + 0.5f);
    if (px < 4) px = 4;
    sp = SP(7) * f;
    n = layout_text(text, px, sp, 1, chs, 256, &totalW, &boxH);
    startX = L->stageW / 2.0f - totalW / 2.0f;
    startY = L->stageH / 2.0f - px / 2.0f;

    for (i = 0; i < n && made < MAXPARTS; i++) {
        int j = 0;
        Part *p = NULL;
        for (j = 0; j < MAXPARTS; j++) if (!g_parts[j].used) { p = &g_parts[j]; break; }
        if (!p) break;
        p->used = 1;
        p->cp = chs[i].cp;
        p->x0 = startX + chs[i].x;
        p->y0 = startY;
        p->adv = chs[i].adv;
        p->boxH = boxH;
        p->px = px;
        p->dx = rnd(-120, 120) * g_k;
        p->dy = rnd(-150, 50) * g_k;
        p->scT = rnd(0.3, 0.7);
        p->rotT = rnd(-30, 30);
        p->t0 = now;
        made++;
    }
}

static void update_parts(const Laid *L, double now)
{
    int i;
    for (i = 0; i < MAXPARTS; i++) {
        Part *p = &g_parts[i];
        double t, tp, e, op, bl, s, rot, x, y;
        float xmin, xmax, ymin, ymax;
        char utf8[8];
        if (!p->used) continue;
        if (g_lowQ && (i & 1)) continue;      /* half the debris on a slow CPU */
        t = now - p->t0;
        if (t >= 1000) { p->used = 0; continue; }
        tp = clampd(t / 800.0, 0, 1);
        e = bez(0.25, 1, 0.5, 1, tp);
        op = 1.0 - ease_in(clampd(t / 700.0, 0, 1));
        bl = 4.0 * ease_in(tp);
        s = 1.0 + (p->scT - 1.0) * e;
        rot = p->rotT * e;
        x = p->x0 + p->dx * e;
        y = p->y0 + p->dy * e;
        /* the flight is in stage coordinates; keep it inside the window so no
           glyph is cut in half by the edge of the monitor */
        xmin = -L->stageX;
        xmax = L->winW - L->stageX - p->adv;
        ymin = -L->stageY;
        ymax = L->winH - L->stageY - p->boxH;
        if (x < xmin) x = xmin;
        if (x > xmax) x = xmax;
        if (y < ymin) y = ymin;
        if (y > ymax) y = ymax;
        utf8[u8enc(p->cp, utf8)] = 0;
        draw_line(utf8, p->px > 0 ? p->px : FONT_MAIN, 0, L->stageX + (float)x,
                  L->stageY + (float)y, (float)op,
                  1 /* per-glyph ramp */, 0, (float)bl,
                  g_lowQ ? 0.0f : 0.40f, 6, (float)s, (float)rot, 0);
    }
}

/* ------------------------------ side regions ------------------------------ */
static void eval_frag(const Frag *f, double now, double *size, double *op, double *blur,
                      double *l, double *t, double *scale)
{
    double p = clampd((now - f->tStart) / 1400.0, 0, 1);
    double es = bez(0.22, 1, 0.36, 1, p);
    double pp = clampd((now - f->tStart) / 1500.0, 0, 1);
    double eo = ease_out(pp);
    double bp = clampd((now - f->tStart) / 1600.0, 0, 1);
    double eb = ease_inout(bp);
    *size = f->sz0 + (f->sz1 - f->sz0) * es;
    *op = f->op0 + (f->op1 - f->op0) * eb;
    *blur = f->bl0 + (f->bl1 - f->bl0) * eb;
    *l = f->l0 + (f->l1 - f->l0) * eo;
    *t = f->t0pos + (f->t1pos - f->t0pos) * eo;
    *scale = f->sc0 + (f->sc1 - f->sc0) * es;
}

static void shift_frags(double now)
{
    int i;
    for (i = 0; i < MAXFRAGS; i++) {
        Frag *f = &g_frags[i];
        double sz, op, bl, l, t, sc;
        if (!f->used) continue;
        if (f->tier >= 3) { f->used = 0; continue; }
        eval_frag(f, now, &sz, &op, &bl, &l, &t, &sc);
        f->sz0 = sz; f->op0 = op; f->bl0 = bl; f->l0 = l; f->t0pos = t; f->sc0 = sc;
        f->tier++;
        f->sz1 = tier_size(f->tier);
        f->op1 = tier_op(f->tier);
        f->bl1 = tier_blur(f->tier);
        f->sc1 = 1.0;
        f->l1 = clampd(l + rnd(-10, 10), -10, 110);
        f->t1pos = clampd(t + rnd(-10, 10), -10, 110);
        f->tStart = now;
    }
}

static void extract_fragments(const char *line, char out[FRAG_MAXN][128], int *nOut)
{
    int n = 0, hasLatin = 0, cap = g_fragCntN;
    const char *p;
    u32 c;
    char *q;

    if (cap < FRAG_MIN) cap = FRAG_MIN;
    if (cap > FRAG_MAXN) cap = FRAG_MAXN;

    for (p = line; *p; ) {
        int adv = u8dec(p, &c);
        p += adv;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) { hasLatin = 1; break; }
    }

    if (hasLatin) {
        char tmp[128];
        const char *s = line;
        while (*s && n < cap - 1) {
            int k = 0;
            while (*s && (u8dec(s, &c), is_u8_ascii_space(c))) s += u8dec(s, &c);
            while (*s && (u8dec(s, &c), !is_u8_ascii_space(c))) {
                int adv = u8dec(s, &c);
                if (is_word_ch(c) && k < 120) { memcpy(tmp + k, s, (size_t)adv); k += adv; }
                s += adv;
            }
            tmp[k] = 0;
            if (k > 0) { u8ncpy(out[n], tmp, 128); n++; }
        }
    } else {
        /* strip punctuation, then keep up to the user's character count */
        char tmp[LY_MAX];
        int k = 0;
        for (p = line; *p; ) {
            int adv = u8dec(p, &c);
            p += adv;
            if (is_cjk_punct(c)) continue;
            if (k + 4 < (int)sizeof tmp) k += u8enc(c, tmp + k);
        }
        tmp[k] = 0;
        for (q = tmp; *q && n < cap; ) {
            int adv = u8dec(q, &c);
            if (adv > 4) adv = 4;
            memcpy(out[n], q, (size_t)adv);
            out[n][adv] = 0;
            q += adv;
            n++;
        }
    }
    *nOut = n;
}

static void add_frags(const char *line, double now)
{
    char frags[FRAG_MAXN][128];
    int n = 0, i;
    extract_fragments(line, frags, &n);
    for (i = 0; i < n; i++) {
        int j;
        Frag *f = NULL;
        for (j = 0; j < MAXFRAGS; j++) if (!g_frags[j].used) { f = &g_frags[j]; break; }
        if (!f) break;
        f->used = 1;
        u8ncpy(f->txt, frags[i], sizeof f->txt);
        f->side = (i % 2 == 0) ? 0 : 1;
        f->tier = 1;
        f->l0 = f->l1 = rnd(-5, 105);
        f->t0pos = f->t1pos = rnd(-5, 105);
        f->sz0 = f->sz1 = tier_size(1);
        f->op0 = f->op1 = tier_op(1);
        f->bl0 = f->bl1 = tier_blur(1);
        f->sc0 = 0.9; f->sc1 = 1.0;
        f->tStart = now;
        f->rot = rnd(-40, 40);
        f->dAx = rnd(-7, 7); f->dAy = rnd(-7, 7); f->dPh = rnd(0, 6.283);
    }
}

static void update_sides(const char *line, double now)
{
    shift_frags(now);
    add_frags(line, now);
}

static void draw_frags(const Laid *L, double now, int tier)
{
    int i;
    for (i = 0; i < MAXFRAGS; i++) {
        Frag *f = &g_frags[i];
        double sz, op, bl, l, t, sc, drx, dry;
        int px;
        if (!f->used || f->tier != tier) continue;
        eval_frag(f, now, &sz, &op, &bl, &l, &t, &sc);
        px = (int)(sz * L->fragScale + 0.5);
        if (px < 1) px = 1;
        /* The size animates continuously and the glyph cache is keyed on it, so
           every frame would otherwise mint a fresh bitmap. Round to 4px steps
           and fold the remainder into the draw scale: the resampler hides the
           quantisation, and the working set shrinks enough to live in the
           cache instead of thrashing it. */
        {
            int st = g_lowQ ? 8 : 4;
            int q = (px + st / 2) & ~(st - 1);
            if (q >= st && q != px) { sc *= (double)px / (double)q; px = q; }
        }
        drx = f->dAx * sin(2 * 3.14159265358979 * (now / 6000.0) + f->dPh);
        dry = f->dAy * cos(2 * 3.14159265358979 * (now / 6000.0) + f->dPh);
        {
            float ox = L->sideX[f->side] + (float)(l / 100.0 * L->sideW) + (float)drx;
            float oy = L->stageY + (float)(t / 100.0 * L->stageH) + (float)dry;
            int ncp = 0;
            const char *s;
            float estW, lim;
            /* a whole English word is wider than the CJK glyph the column was
               sized for; keep the word itself inside the window */
            for (s = f->txt; *s; s++)
                if (((unsigned char)*s & 0xC0) != 0x80) ncp++;
            estW = (float)px * (0.62f * (float)ncp + 0.4f) * (float)sc;
            lim = L->winW - 4.0f - estW;
            if (ox > lim) ox = lim;
            if (ox < 4.0f) ox = 4.0f;
            draw_line(f->txt, px, 0, ox, oy, (float)op, 2,
                      f->side == 0 ? SOLID_LEFT : SOLID_RIGHT, (float)bl, 0, 0,
                      (float)sc, (float)f->rot, 0);
        }
    }
}

/* --------------------------- main stage drawing --------------------------- */
static void draw_current(const Laid *L, double now)
{
    double size, alpha, yoff, spacing, blur, drift = 0, driftY = 0;
    float totalW, boxH;
    float ox, oy;

    if (g_phase == PH_NONE || !g_cur[0]) return;

    if (g_phase == PH_FLY) {
        double p = ease_linear((now - g_phaseT0) / 600.0);
        /* the fly-in ends on the resting size, whatever the band scale made it:
           a literal 21->42 here overshoots FONT_MAIN on a short band and the line
           visibly snaps smaller the moment the animation hands over to spread */
        size = FONT_MAIN * (0.5 + 0.5 * p);
        alpha = 0.6 + 0.4 * p;
        yoff = PEND_DY * (1 - p);
        spacing = 3;
        blur = 1.0 * (1 - p);
        if (now - g_phaseT0 >= 600) { g_phase = PH_SPREAD; g_phaseT0 = now; }
    } else if (g_phase == PH_SPREAD) {
        double p = ease_linear((now - g_phaseT0) / 2000.0);
        size = FONT_MAIN;
        alpha = 1;
        yoff = 0;
        spacing = 3 + 4 * p;
        blur = 0;
        if (now - g_phaseT0 >= 2000) {
            g_phase = PH_IDLE;
            g_phaseT0 = now;
            g_driftOn = 1;
            g_driftT0 = now;
            g_driftAx = rnd(-7, 7);
            g_driftAy = rnd(-7, 7);
            g_pendDx = rnd(-7, 7);
            g_pendDy = rnd(-7, 7);
        }
    } else {
        size = FONT_MAIN;
        alpha = 1;
        yoff = 0;
        spacing = 7;
        blur = 0;
    }

    if (g_driftOn) {
        double ph = clampd((now - g_driftT0) / 3000.0, 0, 2);
        double tri = ph < 1 ? ph : 2 - ph;
        double es = ease_inout(tri);
        drift = g_driftAx * es;
        driftY = g_driftAy * es;
    }

    {
        int px = (int)(size + 0.5);
        float sp = (float)(spacing * g_k);
        float f = line_fit(L, g_cur, px, sp);
        float sc = (float)(px > 0 ? size / px : 1.0);
        px = (int)(px * f + 0.5f);
        if (px < 4) px = 4;
        sp *= f;
        totalW = line_width(g_cur, px, sp, 0, &boxH);
        ox = L->stageX + L->stageW / 2.0f - totalW / 2.0f + (float)(drift * g_k);
        oy = L->stageY + L->stageH / 2.0f - boxH / 2.0f + (float)(yoff + driftY * g_k);
        draw_line(g_cur, px, sp, ox, oy, (float)alpha, 0, 0,
                  (float)blur, 0.15f, px >= 36 ? 6 : 3, sc, 0, 0);
    }
}

static void draw_pending(const Laid *L, double now)
{
    double a, ph = 0;
    float totalW, boxH, ox, oy;
    if (!g_pend[0]) return;
    a = g_pendFrom + (g_pendTo - g_pendFrom) * ease_out((now - g_pendT0) / 400.0);
    if (a <= 0.004) return;
    if (g_driftOn) {
        double p = clampd((now - g_driftT0) / 3000.0, 0, 2);
        double tri = p < 1 ? p : 2 - p;
        ph = ease_inout(tri);
    }
    {
        int px = FONT_PEND;
        float sp = SP(2);
        float f = line_fit(L, g_pend, px, sp);
        px = (int)(px * f + 0.5f);
        if (px < 4) px = 4;
        sp *= f;
        totalW = line_width(g_pend, px, sp, 0, &boxH);
        ox = L->stageX + L->stageW / 2.0f - totalW / 2.0f + (float)(ph * g_pendDx * g_k);
        oy = L->stageY + L->stageH / 2.0f - boxH / 2.0f + PEND_DY + (float)(ph * g_pendDy * g_k);
        draw_line(g_pend, px, sp, ox, oy, (float)a, 2, SOLID_PENDING, 0, 0, 0, 1, 0, 0);
    }
}

static void play_current(const char *text, const char *next, const Laid *L, double now)
{
    if (g_cur[0]) explode_text(g_cur, L, now);
    g_driftOn = 0;
    u8ncpy(g_cur, text, sizeof g_cur);
    g_phase = PH_FLY;
    g_phaseT0 = now;
    if (next && next[0]) {
        u8ncpy(g_pend, next, sizeof g_pend);
        g_pendFrom = 0;
        g_pendTo = 0.85;
        g_pendT0 = now;
    } else {
        g_pend[0] = 0;
        g_pendTo = 0;
    }
    update_sides(text, now);
}

/* ------------------------------ lyric source ------------------------------ */
static CRITICAL_SECTION g_cs;
static char   g_pubText[LY_MAX] = "", g_pubNext[LY_MAX] = "";
static double g_pubProgressMs = 0;
static int    g_pubPlaying = 0;
static int    g_netOk = 0;      /* successful status fetches */
static int    g_netTries = 0;
static int    g_demo = 0;
static volatile long g_quit = 0;

static const char DEMO_LRC[] =
    "[00:00.00]夜色漫过城市的窗台\r\n"
    "[00:04.00]风把遥远的灯吹亮\r\n"
    "[00:08.00]我听见心跳在回响\r\n"
    "[00:12.00]Hold the light across the sky\r\n"
    "[00:16.00]回声穿过长长的街\r\n"
    "[00:20.00]把所有沉默唱成歌\r\n"
    "[00:24.00]星光落在肩头上\r\n"
    "[00:28.00]我们一起走向明天\r\n";

/* --------------------------- lyric server address -------------------------- */
/*
 * The player service used to be baked into the binary at 127.0.0.1:23330, which
 * only works when the lyric source runs on the same machine. On the Atom tablet
 * it usually runs on another box on the LAN, so the host is editable now. What
 * has to be typed is one address, "192.168.0.100" - the port keeps its 23330
 * default and does not have to be touched. The value is written to skylyric.ini
 * beside the exe so the settings travel with the program.
 */
static void sky_log(const char *line);   /* defined with the other diagnostics */

static char g_host[128] = DEFAULT_HOST;
static int  g_port      = DEFAULT_PORT;
static char g_statusUrl[512] = "";
static char g_lyricUrl[256]  = "";

static void srv_rebuild(void)          /* caller holds g_cs */
{
    sprintf(g_statusUrl, "http://%s:%d" STATUS_PATH, g_host, g_port);
    sprintf(g_lyricUrl,  "http://%s:%d" LYRIC_PATH,  g_host, g_port);
}

/* plain bounded copy, always terminated: the URLs and hosts are short ASCII, and
   a hand-written loop keeps the compiler off the strncpy truncation warnings */
static void cpy_n(char *dst, int cap, const char *src)
{
    int i = 0;
    if (cap <= 0 || !dst) return;
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* the poller may not read the strings while the menu edits them */
static void srv_snapshot(char *status, int scap, char *lyric, int lcap)
{
    EnterCriticalSection(&g_cs);
    if (status) cpy_n(status, scap, g_statusUrl);
    if (lyric) cpy_n(lyric, lcap, g_lyricUrl);
    LeaveCriticalSection(&g_cs);
}

/* "192.168.0.100:23330", for the menu label and the waiting hint */
static void srv_label(char *out, int cap)
{
    if (!out || cap < 2) { if (out && cap > 0) out[0] = 0; return; }
    EnterCriticalSection(&g_cs);
    snprintf(out, (size_t)cap, "%s:%d", g_host, g_port);
    out[cap - 1] = 0;
    LeaveCriticalSection(&g_cs);
}

static int srv_host_ok(const char *h)
{
    size_t i, n = strlen(h);
    if (n == 0 || n > 126) return 0;
    for (i = 0; i < n; i++) {
        char c = h[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) continue;
        if (c == '.' || c == '-' || c == '_') continue;
        return 0;
    }
    return 1;
}

/*
 * Forgiving on purpose: the expected input is a bare address, but people paste
 * what a browser shows them, so an http:// prefix, a ":23330" tail or a trailing
 * path are all tolerated. A port found inside the text wins over the argument;
 * 0 comes back for "no port stated here" and lets the caller's field decide.
 */
static void srv_parse_input(const char *in, char *host, int hcap, int *port)
{
    char t[160];
    const char *s = in ? in : "";
    char *slash, *colon, *q;
    size_t L;

    while (*s == ' ' || *s == '\t') s++;
    if (_memicmp(s, "http://", 7) == 0) s += 7;
    else if (_memicmp(s, "https://", 8) == 0) s += 8;
    cpy_n(t, (int)sizeof t, s);
    q = t;
    slash = strchr(q, '/');
    if (slash) *slash = 0;                 /* the paths belong to us, not to them */
    *port = 0;
    colon = strrchr(q, ':');
    if (colon) {
        *colon = 0;
        *port = atoi(colon + 1);
    }
    L = strlen(q);
    while (L && (q[L - 1] == ' ' || q[L - 1] == '\t')) q[--L] = 0;
    cpy_n(host, hcap, q);
}

static int srv_ini_path(char *out, int cap)
{
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
    char *slash;
    if (n == 0 || n >= (DWORD)cap) return 0;
    slash = strrchr(out, '\\');
    if (!slash) return 0;
    slash[1] = 0;
    if (strlen(out) + 16 >= (size_t)cap) return 0;
    strcat(out, "skylyric.ini");
    return 1;
}

static void srv_save(void)
{
    char ini[MAX_PATH], v[16];
    char host[128];
    int port;
    EnterCriticalSection(&g_cs);
    strcpy(host, g_host);
    port = g_port;
    LeaveCriticalSection(&g_cs);
    if (!srv_ini_path(ini, sizeof ini)) { sky_log("srv: ini path unavailable"); return; }
    _itoa(port, v, 10);
    if (!WritePrivateProfileStringA("lyric", "host", host, ini) ||
        !WritePrivateProfileStringA("lyric", "port", v, ini))
        sky_log("srv: ini write failed (change applies to this run only)");
    else
        sky_log("srv: saved to skylyric.ini");
}

/*
 * Validate, publish, and remember. A port of 0 means "not stated", which falls
 * back to the default so a blank field never strands the display on a nonsense
 * port. 1 = applied, 0 = unusable address, -1 = unusable port; the settings box
 * turns those two failures into different messages.
 */
static int srv_apply(const char *hostText, int port)
{
    char host[128];
    int inlinePort = 0;
    char lbl[192];

    srv_parse_input(hostText, host, sizeof host, &inlinePort);
    if (inlinePort > 0) port = inlinePort;
    if (port <= 0) port = DEFAULT_PORT;
    if (port > 65535) {
        sky_log("srv: rejected, port out of range");
        return -1;
    }
    if (!srv_host_ok(host)) {
        sky_log("srv: rejected, not an address");
        return 0;
    }
    EnterCriticalSection(&g_cs);
    strcpy(g_host, host);
    g_port = port;
    srv_rebuild();
    snprintf(lbl, sizeof lbl, "srv now %s:%d", g_host, g_port);
    LeaveCriticalSection(&g_cs);
    sky_log(lbl);
    srv_save();
    return 1;
}

/* --------------------------- lyric timing offset --------------------------- */
/*
 * Some players report their position a beat early or a beat late, so the lyric
 * can be nudged by up to +-2000 ms in 50 ms steps. The offset is added where the
 * position is read, so the current line, the pending line and the scattered side
 * characters all move together instead of drifting apart.
 */
#define SYNC_STEP 50
#define SYNC_MAX  2000

static int g_offsetMs = 0;
static int g_syncTouched;     /* the ini is only rewritten once the user moved it */

static void sync_set(int v)
{
    v = (int)lround((double)v / SYNC_STEP) * SYNC_STEP;
    if (v > SYNC_MAX) v = SYNC_MAX;
    if (v < -SYNC_MAX) v = -SYNC_MAX;
    EnterCriticalSection(&g_cs);
    g_offsetMs = v;
    LeaveCriticalSection(&g_cs);
}

static int sync_get(void)
{
    int v;
    EnterCriticalSection(&g_cs);
    v = g_offsetMs;
    LeaveCriticalSection(&g_cs);
    return v;
}

static void sync_save(void)
{
    char ini[MAX_PATH], v[16];
    if (!g_syncTouched) return;
    if (!srv_ini_path(ini, sizeof ini)) return;
    _itoa(sync_get(), v, 10);
    if (WritePrivateProfileStringA("sync", "offset", v, ini))
        sky_log("sync: saved to skylyric.ini");
    else
        sky_log("sync: ini write failed (change applies to this run only)");
}

/* --------------------------- display knobs --------------------------- */
/*
 * Three more things are adjusted by watching rather than typing: how big the
 * middle line is, how big the scattered characters in the side regions are, and
 * how many of them one lyric line throws off.
 *
 * The two sizes are percentages of whatever the current mode already picked, not
 * absolute point sizes. That is deliberate: a 1/8 band and a full-screen band
 * start from very different glyph sizes, and "120%" means the same thing in both,
 * so the proportion the modes were tuned for survives the nudge.
 */
#define FS_STEP    5
#define FS_MIN     60
#define FS_MAX     180
#define FS_DEF     100
#define FRAG_DEF   FRAG_PER_LINE

static int g_fsMain  = FS_DEF;
static int g_fsFrag  = FS_DEF;
static int g_fragCnt = FRAG_DEF;
static int g_uiTouched;        /* the ini is only rewritten once the user moved one */

/* the mirrors the compositor reads live next to the size macros that use them */

static void ui_push(void)      /* caller holds g_cs */
{
    g_fsMainF = (float)g_fsMain / 100.0f;
    g_fsFragF = (float)g_fsFrag / 100.0f;
    g_fragCntN = g_fragCnt;
}

static int ui_snap(int v, int step, int lo, int hi)
{
    v = (int)lround((double)v / step) * step;
    if (v > hi) v = hi;
    if (v < lo) v = lo;
    return v;
}

static void ui_set_main(int pct)
{
    pct = ui_snap(pct, FS_STEP, FS_MIN, FS_MAX);
    EnterCriticalSection(&g_cs);
    g_fsMain = pct;
    ui_push();
    LeaveCriticalSection(&g_cs);
}

static void ui_set_frag(int pct)
{
    pct = ui_snap(pct, FS_STEP, FS_MIN, FS_MAX);
    EnterCriticalSection(&g_cs);
    g_fsFrag = pct;
    ui_push();
    LeaveCriticalSection(&g_cs);
}

static void ui_set_count(int n)
{
    n = ui_snap(n, 1, FRAG_MIN, FRAG_MAXN);
    EnterCriticalSection(&g_cs);
    g_fragCnt = n;
    ui_push();
    LeaveCriticalSection(&g_cs);
}

static int ui_get_main(void)
{
    int v;
    EnterCriticalSection(&g_cs);
    v = g_fsMain;
    LeaveCriticalSection(&g_cs);
    return v;
}

static int ui_get_frag(void)
{
    int v;
    EnterCriticalSection(&g_cs);
    v = g_fsFrag;
    LeaveCriticalSection(&g_cs);
    return v;
}

static int ui_get_count(void)
{
    int v;
    EnterCriticalSection(&g_cs);
    v = g_fragCnt;
    LeaveCriticalSection(&g_cs);
    return v;
}

static void ui_save(void)
{
    char ini[MAX_PATH], v[16];
    if (!g_uiTouched) return;
    if (!srv_ini_path(ini, sizeof ini)) return;
    _itoa(ui_get_main(), v, 10); WritePrivateProfileStringA("ui", "main", v, ini);
    _itoa(ui_get_frag(), v, 10); WritePrivateProfileStringA("ui", "frag", v, ini);
    _itoa(ui_get_count(), v, 10); WritePrivateProfileStringA("ui", "count", v, ini);
    sky_log("ui: saved to skylyric.ini");
}

/* read skylyric.ini at startup; a bad or missing file just keeps the defaults */
static void srv_load(void)
{
    char ini[MAX_PATH], host[128];
    int port;

    /* the band can render before any of this works, so start from the default */
    EnterCriticalSection(&g_cs);
    strcpy(g_host, DEFAULT_HOST);
    g_port = DEFAULT_PORT;
    g_offsetMs = 0;
    g_fsMain = FS_DEF;
    g_fsFrag = FS_DEF;
    g_fragCnt = FRAG_DEF;
    ui_push();
    srv_rebuild();
    LeaveCriticalSection(&g_cs);

    if (!srv_ini_path(ini, sizeof ini)) return;
    GetPrivateProfileStringA("lyric", "host", DEFAULT_HOST, host, sizeof host, ini);
    port = (int)GetPrivateProfileIntA("lyric", "port", DEFAULT_PORT, ini);
    if (port <= 0 || port > 65535) port = DEFAULT_PORT;
    if (port != DEFAULT_PORT || strcmp(host, DEFAULT_HOST) != 0) {
        if (srv_apply(host, port) > 0) sky_log("srv taken from skylyric.ini");
        else sky_log("srv: ini holds an unusable address, using default");
    }
    {
        int off = (int)GetPrivateProfileIntA("sync", "offset", 0, ini);
        if (off != 0) {
            char b[64];
            sync_set(off);
            snprintf(b, sizeof b, "sync %+d ms (from skylyric.ini)", sync_get());
            sky_log(b);
        }
    }
    {
        int a = (int)GetPrivateProfileIntA("ui", "main", FS_DEF, ini);
        int f = (int)GetPrivateProfileIntA("ui", "frag", FS_DEF, ini);
        int c = (int)GetPrivateProfileIntA("ui", "count", FRAG_DEF, ini);
        if (a != FS_DEF || f != FS_DEF || c != FRAG_DEF) {
            char b[96];
            ui_set_main(a);
            ui_set_frag(f);
            ui_set_count(c);
            snprintf(b, sizeof b, "ui %d%% / %d%% / %d (from skylyric.ini)",
                     ui_get_main(), ui_get_frag(), ui_get_count());
            sky_log(b);
        }
    }
}

static DWORD WINAPI poller(LPVOID arg)
{
    /* large buffers live in static storage: the default thread stack is 1 MB */
    static char buf[65536];
    static LyricLine lines[MAXLINES];
    int nLines = 0;
    double lastProgressMs = 0;
    double demoT0 = 0;
    (void)arg;

    if (g_demo) {
        nLines = parse_lrc(DEMO_LRC, lines, MAXLINES);
        demoT0 = now_ms();
    }

    for (;;) {
        char text[LY_MAX] = "", next[LY_MAX] = "";
        double progressMs = 0;
        int playing = 0;
        int wait;

        if (g_quit) break;

        if (g_demo) {            double el = now_ms() - demoT0;
            int i;
            long long tms = (long long)fmod(el, 32000.0);
            progressMs = (double)tms;
            for (i = 0; i < nLines; i++) {
                if (lines[i].time <= tms && (i + 1 >= nLines || lines[i + 1].time > tms)) {
                    u8ncpy(text, lines[i].text, sizeof text);
                    if (i + 1 < nLines) u8ncpy(next, lines[i + 1].text, sizeof next);
                    playing = 1;
                    break;
                }
            }
            if (!playing && nLines && tms < lines[0].time) {
                u8ncpy(text, lines[0].text, sizeof text);
                if (nLines > 1) u8ncpy(next, lines[1].text, sizeof next);
                playing = 1;
            }
        } else {
            char su[512], lu[256];
            int got;
            srv_snapshot(su, sizeof su, lu, sizeof lu);
            got = http_get(su, buf, sizeof buf, 700);
            if (got > 0) {
                char statusv[32] = "";
                g_netOk++;
                jstr(buf, "status", statusv, sizeof statusv);
                if (strcmp(statusv, "playing") == 0) {
                    double sec = 0;
                    int off = sync_get();
                    int haveProg = 0;
                    playing = 1;
                    jstr(buf, "lyricLineText", text, sizeof text);
                    if (jnum(buf, "progress", &sec)) { progressMs = sec * 1000.0; haveProg = 1; }
                    /* the user's sync nudge: applied once, here, so every layer
                       of the display moves together with it */
                    progressMs += (double)off;
                    if (progressMs < lastProgressMs - 2000) nLines = 0;
                    lastProgressMs = progressMs;
                    if (text[0]) {
                        int needFetch = 0, i;
                        if (!nLines) needFetch = 1;
                        else {
                            needFetch = 1;
                            for (i = 0; i < nLines; i++)
                                if (strcmp(lines[i].text, text) == 0) { needFetch = 0; break; }
                        }
                        if (needFetch) {
                            int lg = http_get(lu, buf, sizeof buf, 900);
                            if (lg > 0) nLines = parse_lrc(buf, lines, MAXLINES);
                            else nLines = 0;
                        }
                        /* The player names the line it believes is current, and with no
                           offset that is exactly what should be shown. Once the user asks
                           for a nudge, the named line is by definition the wrong one, so
                           the line is taken from the LRC at the shifted position - that is
                           what makes the lyric really arrive earlier or later. Without a
                           reported position, or before the LRC is in hand, the player's own
                           line still wins, because guessing is worse than following. */
                        if (off != 0 && haveProg && nLines > 0) {
                            int cur = -1;
                            for (i = 0; i < nLines; i++)
                                if ((double)lines[i].time <= progressMs) cur = i;
                            if (cur >= 0) u8ncpy(text, lines[cur].text, sizeof text);
                            next[0] = 0;
                            for (i = 0; i < nLines; i++)
                                if ((double)lines[i].time > progressMs) { u8ncpy(next, lines[i].text, sizeof next); break; }
                        } else {
                            for (i = 0; i < nLines; i++) {
                                if ((double)lines[i].time > progressMs) { u8ncpy(next, lines[i].text, sizeof next); break; }
                            }
                        }
                    }
                }
            } else {
                g_netTries++;
            }
        }

        EnterCriticalSection(&g_cs);
        if (playing && text[0]) {
            u8ncpy(g_pubText, text, sizeof g_pubText);
            u8ncpy(g_pubNext, next, sizeof g_pubNext);
            g_pubProgressMs = progressMs;
            g_pubPlaying = 1;
        } else if (!playing) {
            g_pubPlaying = 0;
        }
        LeaveCriticalSection(&g_cs);

        wait = g_demo ? 150 : 400;
        for (; wait > 0 && !g_quit; wait -= 50) Sleep(50);
    }
    return 0;
}

/* --------------------------------- window -------------------------------- */
static HWND g_hwnd;
static u32 *g_bkBuf;
static int g_bw, g_bh;
static int g_fullscreen = 1;
static char g_lastText[LY_MAX] = "";
static int g_hintFrames = 0;

/* defined with the other diagnostics further down; the mode helpers log too */
static void sky_log(const char *line);
/* defined with the tray helpers below; set_mode() needs it */
static void tray_set(int on);
/* defined with the popup menu; set_mode() and band_place() need it */
static void pin_set(int on);

/*
 * Draw on the panel's real pixel grid. Without this the process is DPI-unaware:
 * Windows renders the whole overlay into a virtual 96dpi surface and stretches
 * the result, which is the blur you get on a 133%/150% scaled tablet or laptop,
 * and on a touch screen it also puts the tap targets off by that same factor.
 * Becoming aware means every metric below (monitor rect, taskbar height, band
 * height) comes back in physical pixels, and because every glyph size is derived
 * from the band height the text keeps its apparent size while gaining the extra
 * real pixels to draw it with.
 *
 * The three entry points are tried newest-first: each older one is the only one
 * that exists on an older OS, and the first success wins.
 */
static const char *make_dpi_aware(void)
{
    HMODULE u = GetModuleHandleW(L"user32.dll");
    HMODULE sh;
    /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4 */
    typedef BOOL (WINAPI *SetCtxFn)(HANDLE);
    /* PROCESS_PER_MONITOR_DPI_AWARE == 2 */
    typedef int (WINAPI *SetAwareFn)(int);
    typedef BOOL (WINAPI *SetPlainFn)(void);
    SetCtxFn setCtx;
    SetAwareFn setAware;
    SetPlainFn setPlain;

    if (u) {
        setCtx = (SetCtxFn)(void *)GetProcAddress(u, "SetProcessDpiAwarenessContext");
        if (setCtx && setCtx((HANDLE)-4)) return "dpi: per-monitor v2";
    }
    sh = LoadLibraryW(L"shcore.dll");
    if (sh) {
        setAware = (SetAwareFn)(void *)GetProcAddress(sh, "SetProcessDpiAwareness");
        if (setAware && setAware(2) == 0) return "dpi: per-monitor";
    }
    if (u) {
        setPlain = (SetPlainFn)(void *)GetProcAddress(u, "SetProcessDPIAware");
        if (setPlain && setPlain()) return "dpi: system aware";
    }
    return "dpi: unaware (stretched by the shell)";
}

/* height of the Windows taskbar on the primary monitor, in pixels */
static int taskbar_height(void)
{
    RECT wa;
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0) && wa.bottom > 0) {
        int h = screenH - wa.bottom;
        /* an auto-hidden taskbar reports 0; a lyric that thin is unreadable, so
           fall back to the default 48px strip */
        if (h < 34) h = 48;
        if (h > screenH / 3) h = screenH / 3;
        return h;
    }
    return 48;
}

/* Glyph metrics are authored for the 360px band; scale them to whatever we got.
   The two short bands are authored against their own reference height, because
   /360 leaves the main line at 15px in a 135px band - too small to read. */
static void apply_scale(int h)
{
    double ref = 360.0, k;
    if (g_mode == MODE_TASKBAR)      ref = 78.0;
    else if (g_mode == MODE_EIGHTH)  ref = 220.0;
    k = (double)h / ref;
    if (k > 1.0) k = 1.0;
    if (k < 0.34) k = 0.34;
    g_k = (float)k;
}

static void backbuffer_reset(int w, int h)
{
    if (w < 8 || h < 8) return;
    free(g_bkBuf);
    g_bkBuf = (u32 *)calloc((size_t)w * h, 4);
    if (!g_bkBuf) { g_bw = 0; g_bh = 0; g_dst = NULL; return; }
    g_dst = g_bkBuf;
    g_dstW = g_bw = w;
    g_dstH = g_bh = h;
    apply_scale(h);
}

/*
 * The band is a display, not an application: NOACTIVATE keeps the keyboard off
 * it (a stray ESC used to quit the overlay), TOOLWINDOW keeps it out of Alt+Tab
 * so nobody lands on it by accident, and LAYERED lets the whole band sit over
 * another full-screen window without the desktop compositor clipping it away.
 */
#define BAND_EXSTYLE (WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | \
                      WS_EX_LAYERED)
/* the band is a display overlay, so it sits a little translucent */
#define BAND_ALPHA 235

/* the overlay rectangle for the current mode: full monitor, bottom quarter,
   bottom eighth, or a taskbar-height strip along the bottom edge */
static RECT band_rect(void)
{
    RECT r;
    POINT origin;
    HMONITOR hm;
    MONITORINFO mi;
    int h;
    origin.x = 0; origin.y = 0;
    r.left = 0; r.top = 0;
    r.right = GetSystemMetrics(SM_CXSCREEN);
    r.bottom = GetSystemMetrics(SM_CYSCREEN);
    hm = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    if (hm && GetMonitorInfoW(hm, &mi)) r = mi.rcMonitor;
    h = r.bottom - r.top;
    switch (g_mode) {
    case MODE_FULL:    break;
    case MODE_EIGHTH:  h = h / 8; break;
    case MODE_TASKBAR: h = taskbar_height(); break;
    case MODE_QUARTER:
    default:           h = h / 4; break;
    }
    if (h < 40) h = 40;
    r.top = r.bottom - h;
    return r;
}

/* A full-screen overlay reads better opaque; the short bands read better with a
   little of the desktop showing through. */
static int band_alpha(void)
{
    return g_mode == MODE_FULL ? 255 : BAND_ALPHA;
}

/* Taskbar mode sits exactly on top of the taskbar, so it must not swallow the
   clicks that belong to it. */
static LONG band_exstyle(void)
{
    return (LONG)(BAND_EXSTYLE | (g_mode == MODE_TASKBAR ? WS_EX_TRANSPARENT : 0));
}

/* Re-assert the overlay geometry after a display change or a mode switch: a
   tablet that rotates or drops to a different resolution leaves the old window
   sitting off-screen, which looks exactly like the overlay having vanished. */
static void band_place(void)
{
    RECT b;
    if (!g_hwnd || !g_fullscreen) return;
    b = band_rect();
    /* a display change can hand the window back without the extended styles we
       set at creation; re-assert them along with the geometry */
    SetWindowLongW(g_hwnd, GWL_EXSTYLE, band_exstyle());
    SetWindowPos(g_hwnd, HWND_TOPMOST, b.left, b.top, b.right - b.left,
                 b.bottom - b.top, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    /* after the frame change, so the redraw it forces cannot reset it */
    SetLayeredWindowAttributes(g_hwnd, 0, band_alpha(), LWA_ALPHA);
    {
        RECT cr;
        GetClientRect(g_hwnd, &cr);
        backbuffer_reset(cr.right - cr.left, cr.bottom - cr.top);
    }
    /* the pin hangs off the band's own rectangle, so move it whenever the
       band moves - a rotation or resolution change used to leave it stranded */
    pin_set(g_mode == MODE_TASKBAR);
}

static void toggle_fullscreen(void)
{
    if (g_fullscreen) {
        SetWindowLongW(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowLongW(g_hwnd, GWL_EXSTYLE, 0);
        SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);   /* opaque again */
        ShowWindow(g_hwnd, SW_SHOWNORMAL);
        {
            RECT r = { 0, 0, 1280, 400 };
            AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(g_hwnd, HWND_NOTOPMOST, (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2,
                         60, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED);
        }
        g_fullscreen = 0;
        pin_set(0);           /* a windowed lyric is clickable; no need for a pin */
    } else {
        SetWindowLongW(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        g_fullscreen = 1;
        band_place();
    }
}

/* Switch overlay mode at run time (startup menu, right-click menu, --mode=N). */
static void set_mode(int m)
{
    char lbl[48];
    if (m < MODE_FULL || m > MODE_TASKBAR) return;
    g_mode = m;
    if (!g_fullscreen) {
        SetWindowLongW(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        g_fullscreen = 1;
    }
    band_place();     /* also moves the pushpin in and out with the geometry */
    /* the click-through strip cannot be right-clicked, so hand the user a tray
       entry point for exactly as long as that is true */
    tray_set(g_mode == MODE_TASKBAR);
    /* k is the number that explains every glyph size on a field machine, and it
       is the first thing to look at when the text is the wrong size */
    sprintf(lbl, "mode %d k=%.2f", m, (double)g_k);
    sky_log(lbl);
}

#define IDI_APP 1                 /* must match skylyric.rc */

/* The picture that identifies the program: it lives in the executable's own
   resources (skylyric.rc), so a copied exe keeps it and Explorer, the taskbar
   and the tray all show the same mark. LR_SHARED hands back a cached handle we
   must not destroy, which is exactly what the tray needs for its lifetime. */
static HICON app_icon(int small)
{
    int px = GetSystemMetrics(small ? SM_CXSMICON : SM_CXICON);
    HICON ic = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP),
                                 IMAGE_ICON, px, px, LR_SHARED);
    if (!ic) ic = LoadIcon(NULL, IDI_APPLICATION);
    return ic;
}

/*
 * Notification-area icon, shown only in taskbar mode. That mode is deliberately
 * click-through so the real taskbar stays usable, which leaves no place on the
 * overlay to right-click or tap - the tray icon is the way back in, and unlike a
 * keyboard shortcut it works on a tablet with no keyboard attached.
 */
#define TRAY_UID 1
#define TRAY_MSG (WM_APP + 7)
static BOOL g_trayOn;

static void tray_set(int on)
{
    NOTIFYICONDATAW nid;
    if ((on && g_trayOn) || (!on && !g_trayOn)) return;
    memset(&nid, 0, sizeof nid);
    nid.cbSize = sizeof nid;
    nid.hWnd = g_hwnd;
    nid.uID = TRAY_UID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = TRAY_MSG;
    nid.hIcon = app_icon(1);
    wcscpy(nid.szTip, L"\u5929\u9645\u5c4f\u6b4c\u8bcd");   /* Sky lyric */
    if (on) {
        if (!Shell_NotifyIconW(NIM_ADD, &nid)) return;
        g_trayOn = TRUE;
        sky_log("tray icon on");
    } else {
        Shell_NotifyIconW(NIM_DELETE, &nid);
        g_trayOn = FALSE;
        sky_log("tray icon off");
    }
}

static void render_frame(void)
{
    Laid L;
    double now = now_ms();
    char pubText[LY_MAX], pubNext[LY_MAX];
    int pubPlaying;

    if (!g_dst) return;

    EnterCriticalSection(&g_cs);
    u8ncpy(pubText, g_pubText, sizeof pubText);
    u8ncpy(pubNext, g_pubNext, sizeof pubNext);
    pubPlaying = g_pubPlaying;
    LeaveCriticalSection(&g_cs);

    Laid initL;
    if (pubPlaying && pubText[0] && strcmp(pubText, g_lastText) != 0 &&
        (g_phase == PH_IDLE || g_phase == PH_NONE)) {
        initL = layout_of((float)g_bw, (float)g_bh);
        u8ncpy(g_lastText, pubText, sizeof g_lastText);
        play_current(pubText, pubNext, &initL, now);
    }

    L = layout_of((float)g_bw, (float)g_bh);

    clear_backbuffer();

    if (L.showPend) draw_pending(&L, now);
    draw_current(&L, now);
    update_parts(&L, now);
    if (L.showFrags) {
        draw_frags(&L, now, 3);
        draw_frags(&L, now, 2);
        draw_frags(&L, now, 1);
    }

    if (!g_demo && g_netTries > 12 && g_netOk == 0) {
        /* nothing to show yet: the player status service is not reachable -
           name the address the user actually set, not the built-in default */
        static char hint[192];
        char where[160];
        srv_label(where, sizeof where);
        sprintf(hint, "\xe7\xad\x89\xe5\xbe\x85 %s \xe6\xad\x8c\xe8\xaf\x8d\xe6\x8e\xa5\xe5\x8f\xa3", where);
        /* in a short band there is no spare strip at the bottom: centre it instead */
        float hy = g_bh < 90 ? (float)(g_bh - 22) / 2.0f : (float)g_bh - 40.0f;
        float w = line_width(hint, 16, 0, 0, NULL);
        draw_line(hint, 16, 0, (float)g_bw / 2.0f - w / 2.0f, hy,
                  0.55f, 2, SOLID_LEFT, 0, 0, 0, 1, 0, 0);
        g_hintFrames++;
    }

    {
        static BITMAPINFO bi;
        HDC hdc = GetDC(g_hwnd);
        if (hdc) {
            if (!bi.bmiHeader.biSize) {
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
            }
            bi.bmiHeader.biWidth = g_bw;
            bi.bmiHeader.biHeight = -g_bh;      /* negative = top-down, matches our buffer */
            SetDIBitsToDevice(hdc, 0, 0, g_bw, g_bh, 0, 0, 0, g_bh,
                              g_bkBuf, &bi, DIB_RGB_COLORS);
            ReleaseDC(g_hwnd, hdc);
        }
    }
}

/*
 * Breadcrumb log. The overlay runs on machines nobody can look at, so both ways
 * it can stop - an exception and a clean exit - have to leave a trace beside the
 * executable instead of just vanishing.
 */
static void sky_log(const char *line)
{
    char dir[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, dir, sizeof dir);
    if (n > 0 && n < sizeof dir) {
        char *slash = strrchr(dir, '\\');
        FILE *fp;
        if (slash) slash[1] = 0;
        if (strlen(dir) + 16 >= sizeof dir) return;
        strcat(dir, "skylyric.log");
        fp = fopen(dir, "a");
        if (fp) { fprintf(fp, "%s\n", line); fclose(fp); }
    }
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
    char msg[256];
    sprintf(msg, "unhandled exception code=0x%08lX addr=0x%08lX "
                 "cacheEntries=%u cacheKB=%u",
            (unsigned long)ep->ExceptionRecord->ExceptionCode,
            (unsigned long)(size_t)ep->ExceptionRecord->ExceptionAddress,
            g_cacheCount, (unsigned)(g_cacheBytes / 1024));
    sky_log(msg);
    return EXCEPTION_EXECUTE_HANDLER;
}

#define IDM_TOGGLE 1
#define IDM_CLEAR  2
#define IDM_QUIT   3
#define IDM_MODE1  11
#define IDM_MODE2  12
#define IDM_MODE3  13
#define IDM_MODE4  14
#define IDM_SERVER    21      /* edit the lyric service address */
#define IDM_SRV_RESET 22      /* back to 127.0.0.1:23330 */
#define IDM_SYNC       23     /* open the adjust panel on the timing row */
#define IDM_SYNC_RESET 24     /* nudge back to zero */
#define IDM_ADJ_MAIN   25     /* middle line font size */
#define IDM_ADJ_FRAG   26     /* scattered character font size */
#define IDM_ADJ_COUNT  27     /* how many scattered characters per line */
#define IDM_ADJ_RESET  28     /* every knob back to what the mode picks itself */

/* the address is held as UTF-8 (it is what http_get wants); menu text is wide */
static void a2w(const char *a, WCHAR *w, int cap)
{
    MultiByteToWideChar(CP_UTF8, 0, a, -1, w, cap);
    w[cap - 1] = 0;
}

static void show_menu(HWND h)
{
    HMENU m = CreatePopupMenu();
    HMENU sm = CreatePopupMenu();
    HMENU sett = CreatePopupMenu();
    POINT pt;
    WCHAR line[192];
    char lbl[160];
    if (!m) return;
    AppendMenuW(sm, MF_STRING | (g_mode == MODE_FULL ? MF_CHECKED : 0), IDM_MODE1,
                L"\u5168\u5c4f\u663e\u793a\tCtrl+Alt+1");              /* full */
    AppendMenuW(sm, MF_STRING | (g_mode == MODE_QUARTER ? MF_CHECKED : 0), IDM_MODE2,
                L"\u5c4f\u5e55\u5e95\u90e8 1/4\tCtrl+Alt+2");          /* bottom 1/4 */
    AppendMenuW(sm, MF_STRING | (g_mode == MODE_EIGHTH ? MF_CHECKED : 0), IDM_MODE3,
                L"\u5c4f\u5e55\u5e95\u90e8 1/8\tCtrl+Alt+3");          /* bottom 1/8 */
    /* taskbar mode is click-through on purpose, so say how to reach the menu */
    AppendMenuW(sm, MF_STRING | (g_mode == MODE_TASKBAR ? MF_CHECKED : 0), IDM_MODE4,
                L"\u4efb\u52a1\u680f\u9ad8\u5ea6\tCtrl+Alt+4");        /* taskbar height */
    AppendMenuW(sm, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sm, MF_STRING | MF_GRAYED, 0,
                L"\u4efb\u52a1\u680f\u6a21\u5f0f\u7a7f\u900f\u53ef\u70b9\u51fb"
                L"\uff0c\u70b9\u4e0a\u65b9\u56fe\u9486\u6216\u6309 Ctrl+Alt+/");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sm,
                L"\u663e\u793a\u6a21\u5f0f");                                   /* display mode */
    AppendMenuW(m, MF_STRING, IDM_TOGGLE,
                L"\u5207\u6362\u4e3a\u7a97\u53e3 / \u94fa\u6ee1\u5e95\u90e8");   /* toggle */
    AppendMenuW(m, MF_STRING, IDM_CLEAR,
                L"\u6e05\u9664\u6f02\u6d6e\u5b57");                              /* clear floats */
    /* where the lyrics come from: editable here, remembered in skylyric.ini */
    srv_label(lbl, sizeof lbl);
    a2w(lbl, line, (int)(sizeof line / sizeof line[0]));
    {
        WCHAR shown[224];
        const WCHAR *pre = L"\u5f53\u524d ";                        /* "current " */
        lstrcpynW(shown, pre, (int)(sizeof shown / sizeof shown[0]));
        lstrcatW(shown, line);
        if (sett) AppendMenuW(sett, MF_STRING | MF_GRAYED, 0, shown);
        else AppendMenuW(m, MF_STRING | MF_GRAYED, 0, shown);
    }
    if (sett) AppendMenuW(sett, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_SERVER,
                L"\u8bbe\u7f6e\u6b4c\u8bcd\u5730\u5740\u2026");              /* set address... */
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_SRV_RESET,
                L"\u6062\u590d\u9ed8\u8ba4 127.0.0.1:23330");               /* restore default */
    /* timing: the panel is the practical control, but the value belongs in the
       menu too, so it can be checked without opening anything */
    AppendMenuW(sett ? sett : m, MF_SEPARATOR, 0, NULL);
    {
        WCHAR shown2[224];
        char line2[96];
        const WCHAR *pre2 = L"\u8fdb\u5ea6\u504f\u79fb ";      /* "timing offset " */
        sprintf(line2, "%+d ms", sync_get());
        a2w(line2, line, (int)(sizeof line / sizeof line[0]));
        lstrcpynW(shown2, pre2, (int)(sizeof shown2 / sizeof shown2[0]));
        lstrcatW(shown2, line);
        AppendMenuW(sett ? sett : m, MF_STRING | MF_GRAYED, 0, shown2);
    }
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_SYNC,
                L"\u6b4c\u8bcd\u663e\u793a\u5fae\u8c03\u2026");              /* fine-tune display */
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_SYNC_RESET,
                L"\u504f\u79fb\u5f52\u96f6");                               /* nudge back to zero */
    AppendMenuW(sett ? sett : m, MF_SEPARATOR, 0, NULL);
    /* the three display knobs, each opening the panel on its own row; the values
       are readable right here so nothing has to be opened just to check them */
    {
        WCHAR shown3[224];
        wsprintfW(shown3, L"\u5b57\u53f7 %d \uff05 / %d \uff05\uff0c"
                          L"\u79bb\u6563\u5b57 %d",
                  ui_get_main(), ui_get_frag(), ui_get_count());
        AppendMenuW(sett ? sett : m, MF_STRING | MF_GRAYED, 0, shown3);
    }
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_ADJ_MAIN,
                L"\u4e3b\u6b4c\u8bcd\u5b57\u53f7\u2026");                   /* main lyric size... */
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_ADJ_FRAG,
                L"\u79bb\u6563\u5b57\u5b57\u53f7\u2026");                   /* side character size... */
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_ADJ_COUNT,
                L"\u79bb\u6563\u5b57\u4e2a\u6570\u2026");                   /* side character count... */
    AppendMenuW(sett ? sett : m, MF_STRING, IDM_ADJ_RESET,
                L"\u5168\u90e8\u6062\u590d\u9ed8\u8ba4");                   /* restore all defaults */
    if (sett) AppendMenuW(m, MF_POPUP, (UINT_PTR)sett, L"\u8bbe\u7f6e");      /* settings */
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_QUIT,
                L"\u9000\u51fa  (Ctrl+Alt+Q)");                                   /* quit */
    GetCursorPos(&pt);
    /* TrackPopupMenu needs us foreground or the menu sticks open after a click away */
    SetForegroundWindow(h);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
    DestroyMenu(sm);
    if (sett) DestroyMenu(sett);
    DestroyMenu(m);
}

/* ------------------------------ startup menu ------------------------------ */
/*
 * A chooser shown before the overlay exists, so the user can say how much of
 * the screen it should take. It is open for MENU_MS milliseconds and then picks
 * the default on its own.
 *
 * Custom-drawn rather than built from BUTTON controls on purpose: one window
 * receives the mouse, the digit keys and the timer, so there is no focus to
 * lose and nothing for a tablet's shell to interrupt.
 */
#define MENU_MS    5000
#define MENU_TIMER 1
#define MENU_BG    RGB(16, 18, 26)
#define MENU_ROW   RGB(30, 35, 50)
#define MENU_HOT   RGB(52, 66, 104)
#define MENU_LINE  RGB(74, 84, 112)

static HWND g_menuHwnd;
static int  g_menuChoice;      /* 0 = still waiting */
static int  g_menuHover;       /* 1..4, 0 = none */
static int  g_menuS = 100;     /* layout scale, hundredths of a pixel */
static unsigned long long g_menuDeadline;

#define MS(v) ((v) * g_menuS / 100)

static void menu_rows(HWND h, RECT rows[4])
{
    RECT cr;
    int i, x, w, y, rh, gap;
    GetClientRect(h, &cr);
    x = MS(26); w = cr.right - MS(52);
    y = MS(70); rh = MS(44); gap = MS(9);
    for (i = 0; i < 4; i++)
        SetRect(&rows[i], x, y + i * (rh + gap), x + w, y + i * (rh + gap) + rh);
}

static void menu_paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT cr, rows[4], box;
    HFONT big, small;
    HBRUSH br;
    int i, leftms;
    static const WCHAR *label[4] = {
        L"\u6a21\u5f0f 1    \u5168\u5c4f\u663e\u793a",               /* mode 1  full screen */
        L"\u6a21\u5f0f 2    \u5c4f\u5e55\u5e95\u90e8 1/4",           /* mode 2  bottom 1/4 */
        L"\u6a21\u5f0f 3    \u5c4f\u5e55\u5e95\u90e8 1/8",           /* mode 3  bottom 1/8 */
        L"\u6a21\u5f0f 4    \u4efb\u52a1\u680f\u9ad8\u5ea6"          /* mode 4  taskbar height */
    };
    GetClientRect(h, &cr);

    br = CreateSolidBrush(MENU_BG);
    FillRect(dc, &cr, br);
    DeleteObject(br);
    br = CreateSolidBrush(MENU_LINE);
    FrameRect(dc, &cr, br);
    DeleteObject(br);

    big = CreateFontW(MS(17), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                      DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    small = CreateFontW(MS(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    SetBkMode(dc, TRANSPARENT);

    /* title */
    SelectObject(dc, big);
    SetTextColor(dc, RGB(236, 241, 250));
    SetRect(&box, MS(26), MS(20), cr.right - MS(26), MS(52));
    DrawTextW(dc, L"\u5929\u9645\u5c4f\u6b4c\u8bcd \u00b7 \u9009\u62e9\u663e\u793a\u6a21\u5f0f",
              -1, &box, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    menu_rows(h, rows);
    SelectObject(dc, small);
    for (i = 0; i < 4; i++) {
        RECT r = rows[i];
        int hot = (g_menuHover == i + 1);
        br = CreateSolidBrush(hot ? MENU_HOT : MENU_ROW);
        FillRect(dc, &r, br);
        DeleteObject(br);
        br = CreateSolidBrush(hot ? RGB(126, 158, 224) : RGB(52, 60, 82));
        FrameRect(dc, &r, br);
        DeleteObject(br);
        SetTextColor(dc, hot ? RGB(255, 255, 255) : RGB(216, 224, 240));
        r.left += MS(14);
        DrawTextW(dc, label[i], -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (i == 1) {
            /* mode 2 is what happens if nobody answers */
            RECT t;
            SetRect(&t, rows[i].left, rows[i].top, rows[i].right - MS(14), rows[i].bottom);
            SetTextColor(dc, RGB(140, 196, 255));
            DrawTextW(dc, L"\u9ed8\u8ba4", -1, &t,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    /* footer: how long is left */
    leftms = (int)(g_menuDeadline - GetTickCount64());
    if (leftms < 0) leftms = 0;
    {
        WCHAR foot[160];
        WCHAR num[12];
        wsprintfW(num, L"%d", (leftms + 999) / 1000);
        wcscpy(foot, L"\u6309\u6570\u5b57\u952e 1-4 \u9009\u62e9\uff0c");   /* press 1-4 to choose, */
        wcscat(foot, num);
        wcscat(foot, L" \u79d2\u540e\u81ea\u52a8\u4ee5\u6a21\u5f0f 2 \u542f\u52a8"); /* s, then mode 2 */
        SelectObject(dc, small);
        SetTextColor(dc, RGB(150, 160, 182));
        SetRect(&box, MS(26), rows[3].bottom + MS(12), cr.right - MS(10), cr.bottom - MS(8));
        DrawTextW(dc, foot, -1, &box, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    DeleteObject(big);
    DeleteObject(small);
    EndPaint(h, &ps);
}

static void menu_choose(int mode)
{
    if (g_menuChoice) return;
    g_menuChoice = mode;
    if (g_menuHwnd) KillTimer(g_menuHwnd, MENU_TIMER);
    PostQuitMessage(0);
}

static int menu_hit(HWND h, int x, int y)
{
    RECT rows[4];
    int i;
    menu_rows(h, rows);
    for (i = 0; i < 4; i++)
        if (x >= rows[i].left && x < rows[i].right &&
            y >= rows[i].top && y < rows[i].bottom) return i + 1;
    return 0;
}

static LRESULT CALLBACK menuProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g_menuHwnd = h;
        return 0;
    case WM_PAINT:
        menu_paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;                       /* WM_PAINT covers every pixel */
    case WM_TIMER:
        if ((UINT_PTR)w == MENU_TIMER) {
            if (GetTickCount64() >= g_menuDeadline) { menu_choose(MODE_QUARTER); return 0; }
            {
                RECT rows[4], cr;
                GetClientRect(h, &cr);
                menu_rows(h, rows);
                SetRect(&cr, 0, rows[3].bottom - MS(4), cr.right, cr.bottom);
                InvalidateRect(h, &cr, TRUE);   /* only the countdown ticks */
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP: {
        int row = menu_hit(h, GET_X_LPARAM(l), GET_Y_LPARAM(l));
        if (row) menu_choose(row);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int row = menu_hit(h, GET_X_LPARAM(l), GET_Y_LPARAM(l));
        if (row != g_menuHover) {
            RECT rows[4];
            g_menuHover = row;
            menu_rows(h, rows);
            InvalidateRect(h, &rows[0], TRUE);
            InvalidateRect(h, &rows[1], TRUE);
            InvalidateRect(h, &rows[2], TRUE);
            InvalidateRect(h, &rows[3], TRUE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if ((u32)w >= '1' && (u32)w <= '4') { menu_choose((int)w - '0' + MODE_FULL - 1); return 0; }
        if ((u32)w == VK_ESCAPE || (u32)w == VK_RETURN || (u32)w == VK_SPACE) {
            menu_choose(MODE_QUARTER);
            return 0;
        }
        break;
    case WM_CLOSE:
        menu_choose(MODE_QUARTER);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

/* Block until the user has picked a mode, or MENU_MS has passed. */
static int show_mode_menu(void)
{
    WNDCLASSW wc;
    MSG msg;
    HWND h;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    double s = sh / 1080.0;
    if (s < 0.9) s = 0.9;
    if (s > 1.7) s = 1.7;
    g_menuS = (int)(s * 100.0 + 0.5);
    g_menuChoice = 0;
    g_menuHover = 0;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = menuProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"SkyModeMenu";
    RegisterClassW(&wc);

    g_menuHwnd = h = CreateWindowExW(WS_EX_TOPMOST, L"SkyModeMenu", L"SkyLyric",
                                     WS_POPUP | WS_VISIBLE,
                                     (sw - MS(440)) / 2, (sh - MS(300)) / 2,
                                     MS(440), MS(300), NULL, NULL,
                                     GetModuleHandleW(NULL), NULL);
    if (!h) return MODE_QUARTER;
    g_menuDeadline = GetTickCount64() + MENU_MS;
    SetTimer(h, MENU_TIMER, 100, NULL);
    SetForegroundWindow(h);
    SetActiveWindow(h);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    KillTimer(h, MENU_TIMER);
    DestroyWindow(h);
    g_menuHwnd = NULL;
    return g_menuChoice ? g_menuChoice : MODE_QUARTER;
}

/* --------------------------- address editor --------------------------- */
/*
 * Hand-built rather than a dialog template: there is no .rc and no common
 * controls in this program, and matching the dark look of the menus matters
 * more than a free TAB order. Two fields - address and port - and the port can
 * be left exactly as it is, because 23330 is still the default. It runs modal
 * the way the menus do, so the band simply holds its last frame while the box
 * is open and the poller keeps working in the background.
 */
static HWND    g_srvHwnd, g_srvHostEd, g_srvPortEd;
static int     g_srvDone;          /* 0 = open, 1 = accepted, 2 = cancelled */
static int     g_srvHover;         /* 1 = ok, 2 = cancel */
static int     g_srvS = 100;       /* layout scale, hundredths of a pixel */
static HFONT   g_srvFont, g_srvSmall;
static WNDPROC g_srvEdit0;         /* the stock edit proc, shared by both fields */

#define SS(v) ((v) * g_srvS / 100)

typedef struct {
    RECT title, hint, hostLab, hostEd, portLab, portEd, portNote, ok, no;
} SrvR;

static void srv_mk(RECT *rc, int x, int y, int w, int h)
{
    rc->left = SS(x); rc->top = SS(y);
    rc->right = SS(x + w); rc->bottom = SS(y + h);
}

static void srv_rects(SrvR *r)
{
    srv_mk(&r->title,    20,  16, 340, 26);
    srv_mk(&r->hint,     20,  46, 340, 20);
    srv_mk(&r->hostLab,  20,  78, 340, 18);
    srv_mk(&r->hostEd,   20,  98, 340, 30);
    srv_mk(&r->portLab,  20, 140, 220, 18);
    srv_mk(&r->portEd,   20, 160,  96, 30);
    srv_mk(&r->portNote, 126, 162, 234, 26);
    srv_mk(&r->ok,      206, 206,  74, 30);
    srv_mk(&r->no,      286, 206,  74, 30);
}

static void srv_accept(void);

static LRESULT CALLBACK srvEditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN) {
        if (w == VK_RETURN) { srv_accept(); return 0; }
        if (w == VK_ESCAPE) { g_srvDone = 2; return 0; }
        if (w == VK_TAB) {
            SetFocus(h == g_srvHostEd ? g_srvPortEd : g_srvHostEd);
            return 0;
        }
    }
    return CallWindowProcW(g_srvEdit0, h, m, w, l);
}

static void srv_paint(HWND h)
{
    static const WCHAR *titleTxt =
        L"\u6b4c\u8bcd\u670d\u52a1\u5730\u5740";                    /* lyric service address */
    static const WCHAR *hintTxt =
        L"\u4f8b\u5982 192.168.0.100\uff0c\u7aef\u53e3\u53ef\u4ee5\u4e0d\u6539";
    static const WCHAR *hostTxt = L"\u5730\u5740";                  /* address */
    static const WCHAR *portTxt =
        L"\u7aef\u53e3\uff08\u9ed8\u8ba4 23330\uff09";               /* port (default 23330) */
    static const WCHAR *noteTxt =
        L"\u7559\u7a7a\u5219\u7528 23330";                           /* blank means 23330 */
    static const WCHAR *okTxt   = L"\u786e\u5b9a";                  /* ok */
    static const WCHAR *noTxt   = L"\u53d6\u6d88";                  /* cancel */
    PAINTSTRUCT ps;
    HDC dc;
    RECT cl;
    HBRUSH br;
    SrvR r;
    int i;

    dc = BeginPaint(h, &ps);
    GetClientRect(h, &cl);
    srv_rects(&r);
    br = CreateSolidBrush(MENU_BG);
    FillRect(dc, &cl, br);
    DeleteObject(br);
    SetBkMode(dc, TRANSPARENT);

    SelectObject(dc, g_srvFont);
    SetTextColor(dc, RGB(236, 241, 250));
    DrawTextW(dc, titleTxt, -1, &r.title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(dc, hostTxt, -1, &r.hostLab, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(dc, portTxt, -1, &r.portLab, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(dc, g_srvSmall);
    SetTextColor(dc, RGB(150, 160, 182));
    DrawTextW(dc, hintTxt, -1, &r.hint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(dc, noteTxt, -1, &r.portNote, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    /* the two fields sit on panels we draw: an edit with no border of its own
       keeps the same flat look as the rest of the UI */
    for (i = 0; i < 2; i++) {
        RECT e = i == 0 ? r.hostEd : r.portEd;
        HBRUSH f = CreateSolidBrush(MENU_ROW);
        HPEN pen = CreatePen(PS_SOLID, 1, MENU_LINE);
        HPEN oldP = (HPEN)SelectObject(dc, pen);
        HBRUSH oldB = (HBRUSH)SelectObject(dc, f);
        Rectangle(dc, e.left, e.top, e.right, e.bottom);
        SelectObject(dc, oldP);
        SelectObject(dc, oldB);
        DeleteObject(f);
        DeleteObject(pen);
    }

    for (i = 1; i <= 2; i++) {
        RECT b = i == 1 ? r.ok : r.no;
        HBRUSH f = CreateSolidBrush(i == g_srvHover ? MENU_HOT : MENU_ROW);
        HPEN pen = CreatePen(PS_SOLID, 1, MENU_LINE);
        HPEN oldP = (HPEN)SelectObject(dc, pen);
        HBRUSH oldB = (HBRUSH)SelectObject(dc, f);
        SelectObject(dc, g_srvFont);
        SetTextColor(dc, i == g_srvHover ? RGB(255, 255, 255) : RGB(216, 224, 240));
        Rectangle(dc, b.left, b.top, b.right, b.bottom);
        DrawTextW(dc, i == 1 ? okTxt : noTxt, -1, &b,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldP);
        SelectObject(dc, oldB);
        DeleteObject(f);
        DeleteObject(pen);
    }
    EndPaint(h, &ps);
}

/* read both fields, validate, publish, remember; stays open on a bad address */
static void srv_accept(void)
{
    WCHAR wh[160], wp[24];
    char host[160];
    int port, r;

    if (!g_srvHostEd || !IsWindow(g_srvHostEd)) { g_srvDone = 1; return; }
    GetWindowTextW(g_srvHostEd, wh, (int)(sizeof wh / sizeof wh[0]));
    GetWindowTextW(g_srvPortEd, wp, (int)(sizeof wp / sizeof wp[0]));
    port = (int)wcstol(wp, NULL, 10);           /* blank -> 0 -> the default port */
    host[0] = 0;
    WideCharToMultiByte(CP_UTF8, 0, wh, -1, host, (int)sizeof host, NULL, NULL);
    r = srv_apply(host, port);
    if (r <= 0) {
        /* say which field is wrong: a refused port should not read as a bad IP */
        const WCHAR *badHost =
            L"\u5730\u5740\u65e0\u6548\uff1a\u53ea\u80fd\u662f IP \u6216\u4e3b\u673a\u540d"
            L"\uff0c\u4f8b\u5982 192.168.0.100";
        const WCHAR *badPort =
            L"\u7aef\u53e3\u65e0\u6548\uff1a\u8bf7\u586b 1 \u5230 65535\uff0c"
            L"\u7559\u7a7a\u5219\u7528 23330";
        MessageBoxW(g_srvHwnd, r == 0 ? badHost : badPort,
                    L"\u5929\u9645\u5c4f\u6b4c\u8bcd", MB_OK | MB_ICONWARNING);
        if (r == 0) {
            SendMessageW(g_srvHostEd, EM_SETSEL, 0, -1);
            SetFocus(g_srvHostEd);
        } else {
            SendMessageW(g_srvPortEd, EM_SETSEL, 0, -1);
            SetFocus(g_srvPortEd);
        }
        return;
    }
    g_srvDone = 1;
}

static LRESULT CALLBACK srvProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        srv_paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        static HBRUSH field;
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(236, 241, 250));
        SetBkColor(dc, MENU_ROW);
        if (!field) field = CreateSolidBrush(MENU_ROW);
        return (LRESULT)field;
    }
    case WM_MOUSEMOVE: {
        POINT p;
        SrvR r;
        int hot = 0;
        p.x = GET_X_LPARAM(l); p.y = GET_Y_LPARAM(l);
        srv_rects(&r);
        if (PtInRect(&r.ok, p)) hot = 1;
        else if (PtInRect(&r.no, p)) hot = 2;
        if (hot != g_srvHover) { g_srvHover = hot; InvalidateRect(h, NULL, FALSE); }
        {
            TRACKMOUSEEVENT t;
            memset(&t, 0, sizeof t);
            t.cbSize = sizeof t;
            t.hwndTrack = h;
            t.dwFlags = TME_LEAVE;
            TrackMouseEvent(&t);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_srvHover) { g_srvHover = 0; InvalidateRect(h, NULL, FALSE); }
        return 0;
    case WM_SETCURSOR: {
        POINT p;
        SrvR r;
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        srv_rects(&r);
        SetCursor(LoadCursor(NULL,
                             (PtInRect(&r.ok, p) || PtInRect(&r.no, p)) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_LBUTTONUP: {
        POINT p;
        SrvR r;
        p.x = GET_X_LPARAM(l); p.y = GET_Y_LPARAM(l);
        srv_rects(&r);
        if (PtInRect(&r.no, p)) { g_srvDone = 2; return 0; }
        if (PtInRect(&r.ok, p)) { srv_accept(); return 0; }
        /* a tap on the panel but off a field still means "put the caret somewhere" */
        if (PtInRect(&r.hostEd, p)) SetFocus(g_srvHostEd);
        else if (PtInRect(&r.portEd, p)) SetFocus(g_srvPortEd);
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_RETURN) { srv_accept(); return 0; }
        if (w == VK_ESCAPE) { g_srvDone = 2; return 0; }
        return 0;
    case WM_CLOSE:
        g_srvDone = 2;
        return 0;
    case WM_DESTROY:
        if (g_srvHwnd == h) g_srvHwnd = NULL;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

static void srv_dialog(HWND owner)
{
    static BOOL triedClass, haveClass;
    WNDCLASSW wc;
    MSG msg;
    HINSTANCE inst = GetModuleHandleW(NULL);
    RECT mon;
    SrvR r;
    int sw, sh, x, y, port;
    char hostA[128];
    WCHAR hostW[160], portW[24];
    HINSTANCE hi = inst;

    if (!triedClass) {
        triedClass = TRUE;
        memset(&wc, 0, sizeof wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = srvProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"SkyLyricSrv";
        haveClass = RegisterClassW(&wc) != 0;
        if (!haveClass) sky_log("srv box: class failed");
    }
    if (!haveClass) return;
    if (g_srvHwnd && IsWindow(g_srvHwnd)) {           /* already open: raise it */
        SetForegroundWindow(g_srvHwnd);
        return;
    }

    sh = GetSystemMetrics(SM_CYSCREEN);
    {
        double s = sh / 1080.0;
        if (s < 0.9) s = 0.9;
        if (s > 1.7) s = 1.7;
        g_srvS = (int)(s * 100.0 + 0.5);
    }

    /* prefill with what is live now, so only the part that changed is typed */
    EnterCriticalSection(&g_cs);
    strcpy(hostA, g_host);
    port = g_port;
    LeaveCriticalSection(&g_cs);
    a2w(hostA, hostW, (int)(sizeof hostW / sizeof hostW[0]));
    wsprintfW(portW, L"%d", port);

    mon.left = 0; mon.top = 0;
    mon.right = GetSystemMetrics(SM_CXSCREEN);
    mon.bottom = GetSystemMetrics(SM_CYSCREEN);
    sw = SS(380); sh = SS(250);
    x = (mon.left + mon.right) / 2 - sw / 2;
    y = (mon.top + mon.bottom) / 2 - sh / 2;
    g_srvDone = 0;
    g_srvHover = 0;
    g_srvFont = CreateFontW(SS(17), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_srvSmall = CreateFontW(SS(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_srvHwnd = CreateWindowExW(WS_EX_TOPMOST, L"SkyLyricSrv",
                                L"\u5929\u9645\u5c4f\u6b4c\u8bcd",
                                WS_POPUP | WS_VISIBLE, x, y, sw, sh,
                                owner, NULL, hi, NULL);
    if (!g_srvHwnd) { sky_log("srv box: create failed"); return; }
    srv_rects(&r);

    g_srvHostEd = CreateWindowExW(0, L"EDIT", hostW,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                  r.hostEd.left + SS(6), r.hostEd.top + SS(5),
                                  r.hostEd.right - r.hostEd.left - SS(12),
                                  r.hostEd.bottom - r.hostEd.top - SS(10),
                                  g_srvHwnd, (HMENU)101, hi, NULL);
    g_srvPortEd = CreateWindowExW(0, L"EDIT", portW,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                                  r.portEd.left + SS(6), r.portEd.top + SS(5),
                                  r.portEd.right - r.portEd.left - SS(12),
                                  r.portEd.bottom - r.portEd.top - SS(10),
                                  g_srvHwnd, (HMENU)102, hi, NULL);
    if (!g_srvHostEd || !g_srvPortEd) {
        sky_log("srv box: fields failed");
        DestroyWindow(g_srvHwnd);
        g_srvHwnd = NULL;
        return;
    }
    SendMessageW(g_srvHostEd, WM_SETFONT, (WPARAM)g_srvFont, TRUE);
    SendMessageW(g_srvPortEd, WM_SETFONT, (WPARAM)g_srvFont, TRUE);
    SendMessageW(g_srvHostEd, EM_LIMITTEXT, 127, 0);
    SendMessageW(g_srvPortEd, EM_LIMITTEXT, 5, 0);
    g_srvEdit0 = (WNDPROC)SetWindowLongPtrW(g_srvHostEd, GWLP_WNDPROC, (LONG_PTR)srvEditProc);
    SetWindowLongPtrW(g_srvPortEd, GWLP_WNDPROC, (LONG_PTR)srvEditProc);

    SetForegroundWindow(g_srvHwnd);
    SetActiveWindow(g_srvHwnd);
    SetFocus(g_srvHostEd);
    SendMessageW(g_srvHostEd, EM_SETSEL, 0, -1);
    sky_log("srv box opened");

    while (!g_srvDone) {
        BOOL q;
        if (!g_srvHwnd || !IsWindow(g_srvHwnd)) break;
        q = GetMessageW(&msg, NULL, 0, 0);
        if (q == 0) { PostQuitMessage(0); g_srvDone = 2; break; }   /* quit meant for the app */
        if (q == (BOOL)-1) break;
        if (!IsDialogMessageW(g_srvHwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    /* the edits live inside the box, so dropping the box drops them; the stock
       edit proc we borrowed needs no unhooking */
    if (g_srvHwnd && IsWindow(g_srvHwnd)) DestroyWindow(g_srvHwnd);
    g_srvHwnd = NULL;
    g_srvHostEd = g_srvPortEd = NULL;
    if (g_srvFont) { DeleteObject(g_srvFont); g_srvFont = NULL; }
    if (g_srvSmall) { DeleteObject(g_srvSmall); g_srvSmall = NULL; }
    sky_log(g_srvDone == 1 ? "srv box accepted" : "srv box cancelled");
    if (owner && IsWindow(owner)) {
        SetForegroundWindow(owner);
        InvalidateRect(owner, NULL, FALSE);
    }
}

/* --------------------------- adjust panel --------------------------- */
/*
 * Timing, both font sizes and the fragment count are adjusted by watching, not by
 * typing: this panel is modeless on purpose so the band keeps animating while the
 * buttons are pressed, and a held button keeps stepping. It sits above the band at
 * the left so the lyric line in the middle stays readable while nudging.
 *
 * The four knobs share one panel rather than getting three dialogs each, because
 * they interact: a bigger middle line is usually followed by a bigger, or smaller,
 * scatter of characters, and being able to see both change together is the point.
 */
#define SYNC_ROWS 4
#define ROW_SYNC  0
#define ROW_MAIN  1
#define ROW_FRAG  2
#define ROW_COUNT 3

static HWND  g_syncHwnd;
static int   g_syncHoverRow = -1;    /* row under the pointer, -1 for the bottom buttons */
static int   g_syncHoverBtn;         /* 1 minus, 2 plus, 3 zero, 4 close, 5 label */
static int   g_syncHeldRow = -1;     /* row being held for repeat */
static int   g_syncHeldBtn;          /* 1 or 2 while held */
static int   g_syncRow;              /* the row the keyboard arrows move */
static HFONT g_syncFont, g_syncBig, g_syncSmall;

#define SYNC_TIMER 9
#define SYNC_REPEAT_MS 110

typedef struct { RECT title, hint, label[SYNC_ROWS], minus[SYNC_ROWS],
                       val[SYNC_ROWS], plus[SYNC_ROWS], zero, close; } SyncR;

static void sync_rects(SyncR *r)
{
    int i;
    srv_mk(&r->title, 18, 10, 284, 24);
    srv_mk(&r->hint,  18, 38, 284, 18);
    for (i = 0; i < SYNC_ROWS; i++) {
        int y = 62 + i * 38;
        srv_mk(&r->label[i], 22,  y, 92, 30);
        srv_mk(&r->minus[i], 116, y, 42, 30);
        srv_mk(&r->val[i],   160, y, 78, 30);
        srv_mk(&r->plus[i],  240, y, 42, 30);
    }
    srv_mk(&r->zero,  152, 62 + SYNC_ROWS * 38 + 4, 72, 28);
    srv_mk(&r->close, 230, 62 + SYNC_ROWS * 38 + 4, 52, 28);
}

/* how much one click moves a row: milliseconds, percent, percent, characters */
static int adj_delta(int row)
{
    return row == ROW_SYNC ? SYNC_STEP : (row <= ROW_FRAG ? FS_STEP : 1);
}

static void adj_step(int row, int sign)
{
    static const char *const fmt[SYNC_ROWS] = {
        "sync %+d ms", "ui main %d%%", "ui frag %d%%", "ui count %d"
    };
    char b[64];
    int d = sign * adj_delta(row);
    int before, after, *flag;

    switch (row) {
    case ROW_SYNC: before = sync_get(); sync_set(before + d);
                   after = sync_get(); flag = &g_syncTouched; break;
    case ROW_MAIN: before = ui_get_main(); ui_set_main(before + d);
                   after = ui_get_main(); flag = &g_uiTouched; break;
    case ROW_FRAG: before = ui_get_frag(); ui_set_frag(before + d);
                   after = ui_get_frag(); flag = &g_uiTouched; break;
    default:       before = ui_get_count(); ui_set_count(before + d);
                   after = ui_get_count(); flag = &g_uiTouched; break;
    }
    g_syncRow = row;
    /* at a limit another click moves nothing: stay quiet, and leave the ini alone */
    if (after == before) return;
    *flag = 1;
    if (g_syncHwnd) InvalidateRect(g_syncHwnd, NULL, FALSE);
    snprintf(b, sizeof b, fmt[row], after);
    sky_log(b);
}

/* every knob goes back to what the mode would have used on its own */
static void adj_reset(void)
{
    sync_set(0);
    ui_set_main(FS_DEF);
    ui_set_frag(FS_DEF);
    ui_set_count(FRAG_DEF);
    g_syncTouched = 1;
    g_uiTouched = 1;
    if (g_syncHwnd) InvalidateRect(g_syncHwnd, NULL, FALSE);
    sky_log("adjust: all back to default");
}

static void adj_text(int row, WCHAR *out, int cap)
{
    if (row == ROW_SYNC) {
        int v = sync_get();
        if (v > 0) wsprintfW(out, L"+%d ms", v);
        else if (v < 0) wsprintfW(out, L"%d ms", v);
        else lstrcpyW(out, L"0 ms");
    } else if (row == ROW_MAIN || row == ROW_FRAG) {
        /* the fullwidth percent sign rather than "%%": wsprintfW does not promise
           that a doubled one survives */
        wsprintfW(out, L"%d \uff05", row == ROW_MAIN ? ui_get_main() : ui_get_frag());
    } else {
        wsprintfW(out, L"%d \u4e2a", ui_get_count());      /* n characters */
    }
    out[cap - 1] = 0;
}

/* which control is under the pointer. 0 nothing, 1 minus, 2 plus, 3 zero,
   4 close, 5 label; *row gets the row index, or -1 for the bottom buttons. */
static int sync_hit(const SyncR *r, POINT p, int *row)
{
    int i;
    for (i = 0; i < SYNC_ROWS; i++) {
        if (PtInRect(&r->minus[i], p)) { *row = i; return 1; }
        if (PtInRect(&r->plus[i], p))  { *row = i; return 2; }
        if (PtInRect(&r->label[i], p)) { *row = i; return 5; }
    }
    *row = -1;
    if (PtInRect(&r->zero, p))  return 3;
    if (PtInRect(&r->close, p)) return 4;
    return 0;
}

static const WCHAR *sync_labels[SYNC_ROWS] = {
    L"\u8fdb\u5ea6\u504f\u79fb",           /* timing offset */
    L"\u4e3b\u6b4c\u8bcd\u5b57\u53f7",     /* main lyric font size */
    L"\u79bb\u6563\u5b57\u5b57\u53f7",     /* side character font size */
    L"\u79bb\u6563\u5b57\u4e2a\u6570"      /* side character count */
};

static void sync_paint(HWND h)
{
    static const WCHAR *titleTxt =
        L"\u6b4c\u8bcd\u663e\u793a\u5fae\u8c03";                     /* lyric display */
    static const WCHAR *hintTxt =
        L"\u2212 / + \u70b9\u51fb\u8c03\u6574\uff0c\u6309\u4f4f\u8fde\u7eed"
        L"\uff1b\u5f52\u96f6\u6062\u590d\u9ed8\u8ba4";
    static const WCHAR *minusTxt = L"\u2212";                        /* minus sign */
    static const WCHAR *plusTxt  = L"+";
    static const WCHAR *zeroTxt  = L"\u5f52\u96f6";                 /* to defaults */
    static const WCHAR *closeTxt = L"\u5173\u95ed";                  /* close */
    PAINTSTRUCT ps;
    HDC dc;
    RECT cl;
    SyncR r;
    HBRUSH br;
    int i;

    dc = BeginPaint(h, &ps);
    GetClientRect(h, &cl);
    sync_rects(&r);
    br = CreateSolidBrush(MENU_BG);
    FillRect(dc, &cl, br);
    DeleteObject(br);
    SetBkMode(dc, TRANSPARENT);

    SelectObject(dc, g_syncFont);
    SetTextColor(dc, RGB(236, 241, 250));
    DrawTextW(dc, titleTxt, -1, &r.title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, g_syncSmall);
    SetTextColor(dc, RGB(150, 160, 182));
    DrawTextW(dc, hintTxt, -1, &r.hint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    for (i = 0; i < SYNC_ROWS; i++) {
        int moved = 0;
        WCHAR t[32];

        /* the row the arrows work on gets a bar at its left, so where the next
           keystroke lands is never a guess */
        if (i == g_syncRow) {
            HBRUSH mark = CreateSolidBrush(RGB(140, 196, 255));
            RECT m = r.label[i];
            m.left -= SS(10);
            m.right = m.left + SS(3);
            FillRect(dc, &m, mark);
            DeleteObject(mark);
        }
        SelectObject(dc, g_syncFont);
        SetTextColor(dc, i == g_syncRow ? RGB(236, 241, 250) : RGB(186, 196, 216));
        DrawTextW(dc, sync_labels[i], -1, &r.label[i],
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        adj_text(i, t, (int)(sizeof t / sizeof t[0]));
        moved = (i == ROW_SYNC && sync_get() != 0) ||
                (i == ROW_MAIN && ui_get_main() != FS_DEF) ||
                (i == ROW_FRAG && ui_get_frag() != FS_DEF) ||
                (i == ROW_COUNT && ui_get_count() != FRAG_DEF);
        SelectObject(dc, g_syncFont);
        SetTextColor(dc, moved ? RGB(140, 196, 255) : RGB(196, 206, 226));
        DrawTextW(dc, t, -1, &r.val[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    /* the four rows of -/+ buttons and the two at the bottom are the same thing:
       a filled rectangle, a border and one centred label */
    for (i = 0; i < SYNC_ROWS * 2 + 2; i++) {
        RECT b;
        const WCHAR *txt;
        int hov, row, btn;
        if (i < SYNC_ROWS * 2) {
            row = i / 2;
            btn = (i % 2) + 1;
            b = btn == 1 ? r.minus[row] : r.plus[row];
        } else {
            row = -1;
            btn = i - SYNC_ROWS * 2 + 3;
            b = btn == 3 ? r.zero : r.close;
        }
        txt = btn == 1 ? minusTxt : btn == 2 ? plusTxt : btn == 3 ? zeroTxt : closeTxt;
        hov = (row == g_syncHoverRow && btn == g_syncHoverBtn);
        {
            HBRUSH f = CreateSolidBrush(hov ? MENU_HOT : MENU_ROW);
            HPEN pen = CreatePen(PS_SOLID, 1, MENU_LINE);
            HPEN oldP = (HPEN)SelectObject(dc, pen);
            HBRUSH oldB = (HBRUSH)SelectObject(dc, f);
            SelectObject(dc, btn <= 2 ? g_syncBig : g_syncFont);
            SetTextColor(dc, hov ? RGB(255, 255, 255) : RGB(216, 224, 240));
            Rectangle(dc, b.left, b.top, b.right, b.bottom);
            DrawTextW(dc, txt, -1, &b, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldP);
            SelectObject(dc, oldB);
            DeleteObject(f);
            DeleteObject(pen);
        }
    }
    EndPaint(h, &ps);
}

static LRESULT CALLBACK syncProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    SyncR r;
    POINT p;
    int hit = 0;
    int row = -1;

    switch (m) {
    case WM_PAINT:
        sync_paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        p.x = GET_X_LPARAM(l); p.y = GET_Y_LPARAM(l);
        sync_rects(&r);
        hit = sync_hit(&r, p, &row);
        if (hit != g_syncHoverBtn || row != g_syncHoverRow) {
            g_syncHoverBtn = hit;
            g_syncHoverRow = row;
            InvalidateRect(h, NULL, FALSE);
        }
        {
            TRACKMOUSEEVENT t;
            memset(&t, 0, sizeof t);
            t.cbSize = sizeof t;
            t.hwndTrack = h;
            t.dwFlags = TME_LEAVE;
            TrackMouseEvent(&t);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (g_syncHoverBtn) {
            g_syncHoverBtn = 0;
            g_syncHoverRow = -1;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        sync_rects(&r);
        SetCursor(LoadCursor(NULL, sync_hit(&r, p, &row) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    case WM_LBUTTONDOWN:
        p.x = GET_X_LPARAM(l); p.y = GET_Y_LPARAM(l);
        sync_rects(&r);
        hit = sync_hit(&r, p, &row);
        if (hit == 1 || hit == 2) {
            adj_step(row, hit == 1 ? -1 : 1);
            /* holding a -/+ keeps stepping: forty taps is not how anyone finds a size */
            g_syncHeldRow = row;
            g_syncHeldBtn = hit;
            SetTimer(h, SYNC_TIMER, SYNC_REPEAT_MS, NULL);
        } else if (hit == 3) {
            adj_reset();
        } else if (hit == 4) {
            DestroyWindow(h);
            return 0;
        } else if (hit == 5) {
            g_syncRow = row;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_syncHeldRow >= 0) {
            g_syncHeldRow = -1;
            g_syncHeldBtn = 0;
            KillTimer(h, SYNC_TIMER);
        }
        return 0;
    case WM_TIMER:
        if ((UINT_PTR)w == SYNC_TIMER) {
            if (g_syncHeldRow >= 0 && g_syncHeldBtn)
                adj_step(g_syncHeldRow, g_syncHeldBtn == 1 ? -1 : 1);
            else {
                g_syncHeldRow = -1;
                g_syncHeldBtn = 0;
                KillTimer(h, SYNC_TIMER);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { DestroyWindow(h); return 0; }
        if (w == VK_DOWN || w == VK_TAB) {
            g_syncRow = (g_syncRow + 1) % SYNC_ROWS;
            InvalidateRect(h, NULL, FALSE);
            return 0;
        }
        if (w == VK_UP) {
            g_syncRow = (g_syncRow + SYNC_ROWS - 1) % SYNC_ROWS;
            InvalidateRect(h, NULL, FALSE);
            return 0;
        }
        if (w == VK_LEFT || w == VK_OEM_MINUS || w == VK_SUBTRACT) { adj_step(g_syncRow, -1); return 0; }
        if (w == VK_RIGHT || w == VK_OEM_PLUS || w == VK_ADD)      { adj_step(g_syncRow,  1); return 0; }
        if (w == VK_HOME) { adj_reset(); return 0; }
        return 0;
    case WM_DESTROY:
        KillTimer(h, SYNC_TIMER);
        g_syncHeldRow = -1;
        g_syncHeldBtn = 0;
        g_syncHoverBtn = 0;
        g_syncHoverRow = -1;
        if (g_syncHwnd == h) g_syncHwnd = NULL;
        sync_save();
        ui_save();
        if (g_syncFont) { DeleteObject(g_syncFont); g_syncFont = NULL; }
        if (g_syncBig) { DeleteObject(g_syncBig); g_syncBig = NULL; }
        if (g_syncSmall) { DeleteObject(g_syncSmall); g_syncSmall = NULL; }
        sky_log("adjust panel closed");
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

static void sync_panel(HWND owner, int focusRow)
{
    static BOOL triedClass, haveClass;
    WNDCLASSW wc;
    HINSTANCE inst = GetModuleHandleW(NULL);
    RECT mon;
    int sw, sh, x, y;
    double s;

    if (focusRow < 0) focusRow = 0;
    if (focusRow >= SYNC_ROWS) focusRow = SYNC_ROWS - 1;

    if (!triedClass) {
        triedClass = TRUE;
        memset(&wc, 0, sizeof wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = syncProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"SkyLyricSync";
        haveClass = RegisterClassW(&wc) != 0;
        if (!haveClass) sky_log("adjust panel: class failed");
    }
    if (!haveClass) return;
    if (g_syncHwnd && IsWindow(g_syncHwnd)) {
        /* already open: move it to the row the menu asked for rather than
           stacking a second copy of the same controls */
        g_syncRow = focusRow;
        SetWindowPos(g_syncHwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        InvalidateRect(g_syncHwnd, NULL, FALSE);
        return;
    }

    sh = GetSystemMetrics(SM_CYSCREEN);
    s = sh / 1080.0;
    if (s < 0.9) s = 0.9;
    if (s > 1.7) s = 1.7;
    g_srvS = (int)(s * 100.0 + 0.5);        /* srv_mk() scales off this */

    mon.left = 0; mon.top = 0;
    mon.right = GetSystemMetrics(SM_CXSCREEN);
    mon.bottom = GetSystemMetrics(SM_CYSCREEN);
    sw = SS(300); sh = SS(62 + SYNC_ROWS * 38 + 4 + 28 + 12);
    x = mon.left + SS(16);
    /* clear of the band: the lyric line and the left column of scattered
       characters both live inside it, and the whole point is to watch them while
       stepping. Full-screen mode leaves no room above, so sit inside the top-left
       corner there instead of over the middle. */
    y = band_rect().top - sh - SS(12);
    if (y < mon.top) y = mon.top + SS(12);

    g_syncFont = CreateFontW(SS(17), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_syncBig = CreateFontW(SS(24), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_syncSmall = CreateFontW(SS(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_syncHoverBtn = 0;
    g_syncHoverRow = -1;
    g_syncHeldBtn = 0;
    g_syncHeldRow = -1;
    g_syncRow = focusRow;
    g_syncHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"SkyLyricSync",
                                 L"\u5929\u9645\u5c4f\u6b4c\u8bcd",
                                 WS_POPUP | WS_VISIBLE, x, y, sw, sh,
                                 owner, NULL, inst, NULL);
    if (!g_syncHwnd) { sky_log("adjust panel: create failed"); return; }
    sky_log("adjust panel opened");
}

/* ------------------------------ pushpin tab ------------------------------ */
/*
 * The taskbar strip is deliberately click-through so the real taskbar stays
 * usable, which means the strip itself can never open a menu. This tab is a
 * separate, non-transparent window that hangs just above the middle of the
 * strip: one tap on it brings up the lyric menu, and unlike the hotkey it
 * works on a tablet with no keyboard attached.
 */
static HWND g_pinHwnd;
static int  g_pinHot;
static int  g_pinShown;

static void pin_paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc;
    RECT r;
    HBRUSH br;
    HPEN pen;
    int w, ht, cx, cy, rad;
    COLORREF ink = g_pinHot ? RGB(236, 244, 255) : RGB(168, 184, 214);
    COLORREF fill = g_pinHot ? RGB(96, 132, 210) : RGB(52, 66, 104);

    dc = BeginPaint(h, &ps);
    GetClientRect(h, &r);
    w = r.right - r.left; ht = r.bottom - r.top;
    br = CreateSolidBrush(MENU_BG);
    FillRect(dc, &r, br);
    DeleteObject(br);

    /* a thumbtack, and nothing else: a flat cap, a tapered plastic head, and a
       needle that points down into the lyric strip. No frame, no label. */
    rad = ht * 11 / 50;
    if (rad < 4) rad = 4;
    if (rad > 9) rad = 9;
    cx = w / 2;
    cy = ht / 2 + 1;
    pen = CreatePen(PS_SOLID, 1, ink);
    br = CreateSolidBrush(fill);
    SelectObject(dc, pen);
    SelectObject(dc, br);
    {
        POINT head[4];
        RECT cap;
        head[0].x = cx - rad; head[0].y = cy - rad;
        head[1].x = cx + rad; head[1].y = cy - rad;
        head[2].x = cx + rad / 2; head[2].y = cy + rad / 2;
        head[3].x = cx - rad / 2; head[3].y = cy + rad / 2;
        Polygon(dc, head, 4);
        {
            HBRUSH capBr = CreateSolidBrush(ink);
            cap.left = cx - rad; cap.right = cx + rad + 1;
            cap.top = cy - rad - 3;  cap.bottom = cy - rad;
            FillRect(dc, &cap, capBr);
            DeleteObject(capBr);
        }
        MoveToEx(dc, cx, cy + rad / 2, NULL);
        LineTo(dc, cx, ht - 2);
    }
    DeleteObject(br);
    DeleteObject(pen);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK pinProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        pin_paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return TRUE;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT t;
        if (!g_pinHot) { g_pinHot = 1; InvalidateRect(h, NULL, FALSE); }
        memset(&t, 0, sizeof t);
        t.cbSize = sizeof t;
        t.hwndTrack = h;
        t.dwFlags = TME_LEAVE;
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_pinHot) { g_pinHot = 0; InvalidateRect(h, NULL, FALSE); }
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_CONTEXTMENU:
        sky_log("pin: menu");
        show_menu(g_hwnd);
        /* the menu closed: repaint, in case the click left the tab dirty */
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_DESTROY:
        if (g_pinHwnd == h) g_pinHwnd = NULL;
        g_pinHot = 0;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

/* Show/hide (and always reposition) the pushpin tab. Only taskbar mode uses it. */
static void pin_set(int on)
{
    static BOOL triedClass;
    static BOOL haveClass;
    RECT b;
    int ht, wd, x, y;
    if (!on) {
        if (g_pinHwnd) {
            DestroyWindow(g_pinHwnd);
            g_pinHwnd = NULL;
            g_pinShown = 0;
            sky_log("pin off");
        }
        return;
    }
    if (!triedClass) {
        WNDCLASSEXW wc;
        triedClass = TRUE;
        memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = pinProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"SkyLyricPin";
        haveClass = RegisterClassExW(&wc) != 0;
        if (!haveClass) sky_log("pin: class failed");
    }
    if (!haveClass) return;
    if (!g_pinHwnd) {
        g_pinHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                    L"SkyLyricPin", L"", WS_POPUP,
                                    0, 0, 8, 8, NULL, NULL,
                                    GetModuleHandleW(NULL), NULL);
        if (!g_pinHwnd) { sky_log("pin: create failed"); return; }
    }
    b = band_rect();
    /* a tap target that stays under the finger's reach but never taller than the
       strip it hangs off, and a little wider than tall so the tack reads clearly */
    ht = (b.bottom - b.top) * 62 / 100;
    if (ht < 24) ht = 24;
    if (ht > 38) ht = 38;
    wd = ht * 4 / 3;
    x = (b.left + b.right) / 2 - wd / 2;
    y = b.top - ht;
    SetWindowPos(g_pinHwnd, HWND_TOPMOST, x, y, wd, ht,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!g_pinShown) { g_pinShown = 1; sky_log("pin on"); }
}

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_KEYDOWN:
        /* Band mode must not be killable by a keystroke that was meant for
           whatever the user was actually doing - that is exactly how the overlay
           "suddenly disappeared". Quit on purpose: right-click menu, or
           Ctrl+Alt+Q. ESC still closes the ordinary windowed mode. */
        if ((u32)w == VK_ESCAPE && !g_fullscreen) { sky_log("quit: ESC key"); DestroyWindow(h); }
        else if ((u32)w == VK_F11) toggle_fullscreen();
        else if ((u32)w == 'F') {
            int i;
            for (i = 0; i < MAXPARTS; i++) g_parts[i].used = 0;
            for (i = 0; i < MAXFRAGS; i++) g_frags[i].used = 0;
        }
        return 0;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        show_menu(h);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDM_TOGGLE: toggle_fullscreen(); return 0;
        case IDM_CLEAR: {
            int i;
            for (i = 0; i < MAXPARTS; i++) g_parts[i].used = 0;
            for (i = 0; i < MAXFRAGS; i++) g_frags[i].used = 0;
            return 0;
        }
        case IDM_QUIT: sky_log("quit: menu"); DestroyWindow(h); return 0;
        case IDM_SERVER: srv_dialog(h); return 0;
        case IDM_SRV_RESET:
            /* the escape hatch: a mistyped address should never need the ini */
            if (srv_apply(DEFAULT_HOST, DEFAULT_PORT) > 0) InvalidateRect(h, NULL, FALSE);
            return 0;
        case IDM_SYNC: sync_panel(h, ROW_SYNC); return 0;
        case IDM_SYNC_RESET:
            g_syncTouched = 1;
            sync_set(0);
            sync_save();
            InvalidateRect(h, NULL, FALSE);
            sky_log("sync +0 ms");
            return 0;
        case IDM_ADJ_MAIN:  sync_panel(h, ROW_MAIN);  return 0;
        case IDM_ADJ_FRAG:  sync_panel(h, ROW_FRAG);  return 0;
        case IDM_ADJ_COUNT: sync_panel(h, ROW_COUNT); return 0;
        case IDM_ADJ_RESET:
            adj_reset();
            sync_save();
            ui_save();
            InvalidateRect(h, NULL, FALSE);
            return 0;
        case IDM_MODE1: case IDM_MODE2: case IDM_MODE3: case IDM_MODE4:
            set_mode((int)LOWORD(w) - IDM_MODE1 + MODE_FULL);
            return 0;
        default: break;
        }
        break;
    case TRAY_MSG:
        /* the cursor is already on the tray icon, which is where show_menu puts
           the menu; a left tap opens it too, for tablets without a mouse */
        if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_LBUTTONUP ||
            LOWORD(l) == WM_CONTEXTMENU || LOWORD(l) == WM_LBUTTONDBLCLK) {
            sky_log("tray: menu");
            show_menu(h);
        }
        return 0;
    case WM_HOTKEY:
        /* Taskbar mode is deliberately click-through so the real taskbar still
           works, which means a right-click there belongs to the shell, not to us;
           these keys (and the tray icon) are how you reach the menu instead. */
        if ((u32)w == 1) { sky_log("quit: hotkey"); DestroyWindow(h); return 0; }
        if ((u32)w == 2) { sky_log("hotkey: menu"); show_menu(h); return 0; }
        if ((u32)w >= 10 && (u32)w <= 13) {
            sky_log("hotkey: mode");
            set_mode((int)w - 10 + MODE_FULL);
            return 0;
        }
        break;
    case WM_CLOSE:
        /* Alt+F4 and "show desktop" reach the overlay too; a lyric display that
           can be killed by a stray key is not a lyric display. ESC still quits. */
        if (g_fullscreen) { sky_log("ignored WM_CLOSE"); return 0; }
        break;
    case WM_QUERYENDSESSION:
        sky_log("WM_QUERYENDSESSION (logoff/shutdown)");
        break;
    case WM_ENDSESSION:
        sky_log("WM_ENDSESSION");
        break;
    case WM_DESTROY:
        sky_log("WM_DESTROY");
        tray_set(FALSE);      /* the shell drops the icon when the window dies,
                                 but only on the next refresh: do it now */
        pin_set(0);           /* the tab is a separate window: it dies with us */
        if (g_syncHwnd && IsWindow(g_syncHwnd))
            DestroyWindow(g_syncHwnd);   /* its own WM_DESTROY saves the offset */
        PostQuitMessage(0);
        return 0;
    case WM_SYSCOMMAND:
        /* In band mode the overlay has no title bar, so the only ways to send it
           away are "show desktop" gestures and Alt+F4 - both of which leave it
           minimised forever on a tablet with no obvious way back. */
        if (g_fullscreen && ((w & 0xFFF0) == SC_MINIMIZE ||
                             (w & 0xFFF0) == SC_CLOSE)) return 0;
        break;
    case WM_SIZE:
        if (w == SIZE_MINIMIZED) {
            if (g_fullscreen) ShowWindow(h, SW_RESTORE);
            return 0;
        }
        if (w == SIZE_RESTORED || w == SIZE_MAXIMIZED)
            backbuffer_reset(LOWORD(l), HIWORD(l));
        return 0;
    case WM_DISPLAYCHANGE:
        band_place();
        return 0;
    case WM_DPICHANGED:
        /* per-monitor DPI: the geometry is derived from the monitor itself, so
           re-placing the band is also what rebuilds the backbuffer at the new
           physical size and re-derives every glyph scale from it */
        sky_log("WM_DPICHANGED");
        band_place();
        return 0;
    case WM_POWERBROADCAST:
        /* coming out of sleep, put the band back where it belongs */
        band_place();
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nShow)
{
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    HANDLE th;
    GpStartupInput si;
    RECT mon;
    (void)hPrev; (void)nShow;

    /* before anything measures the screen - and before the startup menu window
       exists - or the shell hands us a virtualised, upscaled display */
    sky_log(make_dpi_aware());

    {
        const char *cl = cmdLine ? cmdLine : "";
        const char *mp;
        int ask = 1;
        if (strstr(cl, "--demo")) g_demo = 1;
        if (strstr(cl, "--window")) { g_fullscreen = 0; ask = 0; }
        mp = strstr(cl, "--mode=");
        if (mp && mp[7] >= '1' && mp[7] <= '4') {
            g_mode = mp[7] - '0';       /* MODE_FULL..MODE_TASKBAR are 1..4 */
            ask = 0;
        }
        if (strstr(cl, "--no-menu")) ask = 0;
        /* five seconds to answer, then the default band takes over */
        if (ask) g_mode = show_mode_menu();
    }

    srand((unsigned)(now_ms() * 1000.0) ^ (unsigned)GetTickCount());
    InitializeCriticalSection(&g_cs);
    srv_load();          /* the lyric service may live on another machine */

    if (gdip_load() != 0) {
        MessageBoxA(NULL, g_gdFail[0] ? g_gdFail : "gdiplus.dll not available", "skylyric", MB_OK);
        return 1;
    }
    memset(&si, 0, sizeof si);
    si.GdiplusVersion = 1;
    if (G_Startup(&g_gdToken, &si, NULL) != 0) {
        MessageBoxA(NULL, "GdiplusStartup failed", "skylyric", MB_OK);
        return 1;
    }

    ramp_lut_init();
    timer_granularity_begin();
    SetUnhandledExceptionFilter(crash_filter);
    { char b[64]; sprintf(b, "start mode=%d", g_mode); sky_log(b); }

    /* font: Microsoft YaHei Light, falling back to YaHei / Segoe UI Light */
    {
        static const WCHAR *names[] = { L"Microsoft YaHei Light", L"Microsoft YaHei",
                                        L"Segoe UI Light", L"PingFang SC", L"Tahoma" };
        size_t i;
        for (i = 0; i < sizeof names / sizeof names[0]; i++) {
            if (G_CreateFontFamilyFromName(names[i], NULL, &g_family) == 0 && g_family) break;
            g_family = NULL;
        }
        if (!g_family) {
            MessageBoxA(NULL, "No usable font found", "skylyric", MB_OK);
            G_Shutdown(g_gdToken);
            return 1;
        }
    }
    G_CreateStringFormat(0, 0, &g_sf);
    G_StringFormatSetAlignment(g_sf, StringAlignmentNear);
    G_StringFormatSetTrimming(g_sf, StringTrimmingNone);

    {
        u32 *scratch = (u32 *)calloc(64 * 64, 4);
        GpBitmap *sb = NULL;
        G_CreateBitmapFromScan0(64, 64, 64 * 4, Format32bppPARGB, scratch, &sb);
        G_CreateFromImage((GpImage *)sb, &g_meas);
        G_SetPageUnit(g_meas, UnitPixel);
        G_SetTextRenderingHint(g_meas, TextRenderingHintAntiAlias);
    }

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hIcon = app_icon(0);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"SkyLyricWindow";
    RegisterClassW(&wc);

    mon = band_rect();

    hwnd = CreateWindowExW(g_fullscreen ? band_exstyle() : 0, L"SkyLyricWindow",
                           L"\u5929\u9645\u5c4f\u6b4c\u8bcd",
                           g_fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW,
                           mon.left, mon.top,
                           g_fullscreen ? (mon.right - mon.left) : 1280,
                           g_fullscreen ? (mon.bottom - mon.top) : 400,
                           NULL, NULL, hInst, NULL);
    g_hwnd = hwnd;
    if (!hwnd) { MessageBoxA(NULL, "CreateWindow failed", "skylyric", MB_OK); return 1; }

    /* A layered window is fully transparent until its attributes are set, so do
       this before the first ShowWindow. */
    if (g_fullscreen) SetLayeredWindowAttributes(hwnd, 0, band_alpha(), LWA_ALPHA);

    ShowWindow(hwnd, g_fullscreen ? SW_SHOWNOACTIVATE : SW_SHOW);
    UpdateWindow(hwnd);
    /* The band refuses ESC (it must not be quit by a keystroke aimed at another
       app), so give the user a global way out that works without focus. */
    if (!RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 'Q'))
        sky_log("RegisterHotKey Ctrl+Alt+Q failed");
    /* ...and a global way in: menus and mode switching from taskbar mode, where
       the overlay is click-through and cannot be right-clicked itself.
       VK_DIVIDE is the numpad slash: Ctrl+Alt+M collides with a hotkey that is
       already taken on this machine, and the digit row is needed by the four
       mode keys below. */
    if (!RegisterHotKey(hwnd, 2, MOD_CONTROL | MOD_ALT, VK_DIVIDE))
        sky_log("RegisterHotKey Ctrl+Alt+/ failed");
    {
        int i;
        for (i = 0; i < 4; i++)
            if (!RegisterHotKey(hwnd, 10 + i, MOD_CONTROL | MOD_ALT, (UINT)('1' + i))) {
                char b[48];
                sprintf(b, "RegisterHotKey Ctrl+Alt+%d failed", i + 1);
                sky_log(b);
            }
    }
    {
        RECT cr;
        char b[96];
        GetClientRect(hwnd, &cr);
        backbuffer_reset(cr.right - cr.left, cr.bottom - cr.top);
        sprintf(b, "band %dx%d k=%.2f", (int)(cr.right - cr.left),
                (int)(cr.bottom - cr.top), (double)g_k);
        sky_log(b);
    }
    tray_set(g_fullscreen && g_mode == MODE_TASKBAR);
    pin_set(g_fullscreen && g_mode == MODE_TASKBAR);

    th = CreateThread(NULL, 0, poller, NULL, 0, NULL);
    (void)th;

    {
        double last = now_ms();
        double avgCost = 0.0, interval = 15.0;
        double lastKeepAlive = last;
        for (;;) {
            double t;
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) goto done;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            t = now_ms();
            /* Another TOPMOST window can slide over us at any time - on a tablet
               that happens whenever a shell or full-screen app repaints. Nudge
               ourselves back to the front without stealing focus. */
            if (t - lastKeepAlive >= 2000.0) {
                lastKeepAlive = t;
                if (g_fullscreen && g_hwnd)
                    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                /* the timing panel is used while the lyric plays, so it must stay
                   above a full-screen band that keeps re-asserting itself */
                if (g_syncHwnd && IsWindow(g_syncHwnd))
                    SetWindowPos(g_syncHwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            if (t - last >= interval) {
                double cost;
                last = t;
                render_frame();
                cost = now_ms() - t;
                avgCost += (cost - avgCost) * 0.10;
                /* On a slow CPU a steady 30fps looks far better than a stuttering
                   attempt at 60, so back off only when frames genuinely overrun. */
                interval = avgCost > 11.0 ? 33.0 : 15.0;
                /* Hysteresis: a machine that needs more than 20ms for a frame is
                   not going to catch up by trying harder, so shed work instead.
                   The desktop path sits near 4ms and never sees this. */
                if (!g_lowQ && avgCost > 20.0) {
                    g_lowQ = 1;
                    sky_log("low-quality mode on");
                } else if (g_lowQ && avgCost < 10.0) {
                    g_lowQ = 0;
                    sky_log("low-quality mode off");
                }
            } else Sleep(1);
        }
    }
done:
    sky_log("exit: window closed");
    UnregisterHotKey(hwnd, 1);
    UnregisterHotKey(hwnd, 2);
    {
        int i;
        for (i = 0; i < 4; i++) UnregisterHotKey(hwnd, 10 + i);
    }
    /* let the poller finish its own loop; killing it outright can leave the CRT
       heap locked and hang the process on the way out */
    InterlockedExchange(&g_quit, 1);
    if (th) {
        int i;
        for (i = 0; i < 40 && WaitForSingleObject(th, 50) == WAIT_TIMEOUT; i++) {}
        TerminateThread(th, 0);
        CloseHandle(th);
    }
    free(g_bkBuf);
    if (g_sf) G_DeleteStringFormat(g_sf);
    if (g_meas) G_DeleteGraphics(g_meas);
    G_Shutdown(g_gdToken);
    DeleteCriticalSection(&g_cs);
    return 0;
}

