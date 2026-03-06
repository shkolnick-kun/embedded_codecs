#include <stdio.h>

#include "g722_tests.h"

int main(void)
{
    printf("G.722 ITU-T compliance tests\n");
    printf("============================\n");
    return itu_compliance_tests();
}
