#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "leptjson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")    //macro parameter 

static void test_parse_null() {
    lept_value v;                                               //v在此处定义，原因是第二个解析函数需要v值
    v.type = LEPT_FALSE;                                        //c不在此处定义，原因相反
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "null"));       //判断解析内容函数返回值是否为LEPT_PARSE_OK
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));                //判断解析类型函数返回值是否是LEPT_NULL
}

static void test_parse_true() {
    lept_value v;
    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "true"));
    EXPECT_EQ_INT(LEPT_TRUE, lept_get_type(&v));
}

static void test_parse_false() {
    lept_value v;
    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "false"));
    EXPECT_EQ_INT(LEPT_FALSE, lept_get_type(&v));
}

#define TEST_ERROR(error, json) \
    do {\
        lept_value v;\
        v.type = LEPT_FALSE;\
        EXPECT_EQ_INT(error, lept_parse(&v, json));\
        EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));\
    } while(0)

static void test_parse_expect_value() {
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value() {
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "?");
}

static void test_parse_root_not_singular() {
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "null x");
}

static void test_parse() {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
}

int main() {
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);      //%3.2f表示数字部分包括小数点占3位，不足3个，前面补空格，超过3个，按实际大小表示；2表示小数部分不包括小数点占2位
    return main_ret;        //正常的main_ret为0, 异常的main_ret为1。 由EQ宏定义代码得出结论
}