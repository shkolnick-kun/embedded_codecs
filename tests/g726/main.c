#include <stdio.h>

#include "g726_tests.h"

int main(void)
{
    printf("G.726 ITU-T compliance tests\n");
    printf("============================\n");
    return itu_compliance_tests();
}
