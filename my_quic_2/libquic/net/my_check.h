#ifndef MY_CHECK_H_
#define MY_CHECK_H_
#include <stdio.h>
#include <stdlib.h>
#define MY_CHECK_IS_ON() 1
#if MY_CHECK_IS_ON()
#define DCHECK_OP(name, op, val1, val2) \
do{\
if(!(val1 op val2))\
{\
	printf("%s,%d\n",__FUNCTION__,__LINE__);\
	abort();\
}\
}while(0)
# else
#define DCHECK_OP(name, op, val1, val2)
#endif
#define DCHECK_EQ(val1, val2) DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) DCHECK_OP(GT, > , val1, val2)

#endif /* MY_CHECK_H_ */
