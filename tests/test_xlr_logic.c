#include "xlr_logic.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(xlr_count_from(XLR_DA_AN, 1) == XLR_DA_AN);
    assert(xlr_count_from(XLR_DA_AN, 6) == XLR_KONG_WANG);
    assert(xlr_count_from(XLR_DA_AN, 7) == XLR_DA_AN);
    assert(xlr_count_from(XLR_CHI_KOU, 4) == XLR_DA_AN);

    xlr_cast_t cast;
    assert(xlr_cast_numbers(1, 1, 1, &cast));
    assert(cast.path[0] == XLR_DA_AN);
    assert(cast.path[1] == XLR_DA_AN);
    assert(cast.path[2] == XLR_DA_AN);

    assert(xlr_cast_numbers(6, 6, 6, &cast));
    assert(cast.path[0] == XLR_KONG_WANG);
    assert(cast.path[1] == XLR_XIAO_JI);
    assert(cast.path[2] == XLR_CHI_KOU);
    assert(!xlr_cast_numbers(0, 1, 1, &cast));
    assert(!xlr_cast_numbers(1, 1, 1, NULL));
    puts("xlr_logic: PASS");
    return 0;
}
