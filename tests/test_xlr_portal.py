#!/usr/bin/env python3
"""Compile the production DNS parser with sanitizers and malformed datagrams."""
from pathlib import Path
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / 'main/xlr_portal.c').read_text().split('#include "xlr_portal.h"')[0]
checks = r'''
#include <assert.h>
int main(void) {
    const uint8_t query[] = {0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'f','o','o',0,0,1,0,1};
    const uint8_t ip[] = {192,168,4,1};
    uint8_t p[512];
    memcpy(p,query,sizeof(query));
    size_t n = dns_reply(p,sizeof(query),sizeof(p),ip);
    assert(n == sizeof(query)+16 && p[0] == 0x12 && p[1] == 0x34 && p[2] == 0x81 && p[7] == 1);
    assert(!memcmp(p+n-4,ip,4));
    memcpy(p,query,sizeof(query)); p[18]=28; /* AAAA -> valid no-data */
    assert(dns_reply(p,sizeof(query),sizeof(p),ip)==sizeof(query) && p[7]==0);
    for (size_t i=0;i<sizeof(query);i++) {
        memcpy(p,query,sizeof(query)); assert(!dns_reply(p,i,sizeof(p),ip));
    }
    memcpy(p,query,sizeof(query)); p[12]=0xc0;
    assert(!dns_reply(p,sizeof(query),sizeof(p),ip));
    memcpy(p,query,sizeof(query)); p[5]=2;
    assert(!dns_reply(p,sizeof(query),sizeof(p),ip));
    memcpy(p,query,sizeof(query));
    assert(!dns_reply(p,sizeof(query),sizeof(query),ip));
    uint32_t random=123;
    for (int i=0;i<20000;i++) {
        for (size_t j=0;j<sizeof(p);j++) { random=random*1664525+1013904223; p[j]=random>>24; }
        dns_reply(p,i%513,sizeof(p),ip);
    }
}
'''
with tempfile.TemporaryDirectory() as folder:
    c = Path(folder)/'dns.c'
    c.write_text(source+checks)
    exe = Path(folder)/'dns'
    subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('xlr_portal: DNS A/AAAA, malformed packets and bounds PASS')
