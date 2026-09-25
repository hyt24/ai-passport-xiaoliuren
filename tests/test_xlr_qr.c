#include "qrcodegen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Uses the same bundled encoder and dimensions as the screen. No real credentials. */
static void render(const char *text, const char *path) {
    unsigned char data[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
    int version = qrcodegen_getMinFitVersion(qrcodegen_Ecc_MEDIUM, strlen(text));
    assert(version > 0);
    memcpy(data, text, strlen(text));
    assert(qrcodegen_encodeBinary(data, strlen(text), qr, qrcodegen_Ecc_MEDIUM,
                                  version, version, qrcodegen_Mask_AUTO, true));
    int modules = qrcodegen_getSize(qr), scale = 160 / modules;
    int margin = 24 + (160 - modules * scale) / 2;
    assert(scale >= 2 && margin >= 4 * scale);
    unsigned char header[54] = {'B', 'M'};
    unsigned size = 54 + 208 * 208 * 3;
    for (int i=0;i<4;i++) header[2+i]=(size>>(8*i))&255;
    header[10]=54; header[14]=40; header[18]=208; header[22]=208;
    header[26]=1; header[28]=24;
    FILE *out=fopen(path,"wb"); assert(out);
    assert(fwrite(header,1,sizeof(header),out)==sizeof(header));
    for (int y=207;y>=0;y--) for (int x=0;x<208;x++) {
        int dx=x-margin,dy=y-margin;
        bool black=dx>=0&&dy>=0&&dx<modules*scale&&dy<modules*scale&&qrcodegen_getModule(qr,dx/scale,dy/scale);
        for(int c=0;c<3;c++) fputc(black?0:255,out);
    }
    fclose(out);
}
int main(void) {
    render("WIFI:T:WPA;S:XiaoLiuRen-TEST;P:0123ABCD;;", "/private/tmp/xlr-wifi-qr.bmp");
    render("http://192.168.4.1", "/private/tmp/xlr-page-qr.bmp");
    render("DPP:C:81/6;M:001122334455;I:XiaoLiuRen;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgAC7Wq8u7goDGmsCGzpdd-zNkBZNdPlLRwsToCQsiFCqXs;;", "/private/tmp/xlr-dpp-qr.bmp");
    puts("QR encoder and quiet-zone checks: PASS");
}
