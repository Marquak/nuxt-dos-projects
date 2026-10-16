#include <stdio.h>
#include "ENGINE.H"
static unsigned tests,failures;
static void check(int ok,const char *s) {++tests;if(!ok){++failures;printf("FAIL: %s\n",s);}}
#include "TESTCORE.H"
int main(void) {coretests();printf("%u checks, %u failures\n",tests,failures);return failures?1:0;}
