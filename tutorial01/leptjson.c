#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <errno.h>  /* errno, ERANGE */
#include <math.h>   /* HUGE_VAL */
#include <stdlib.h>  /* NULL */
#include <stdio.h>
#include <string.h> /* memcpy */

#define LEPT_MALLOC_ERROR -2
#define LEPT_REALLOC_ERROR -3

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif // !LEPT_PARSE_STACK_INIT_SIZE

/* macro : PUTC(c, ch) */
/* usage : 首先拓展空间，然后把ch压栈    */
#define PUTC(c, ch)         do {*(char*)lept_context_push(c, sizeof(char) ) = ch; } while(0)
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {                /*把一个动态堆栈的数据放入lept_context这个结构体*/
    const char* json;
    char* stack;
    size_t size, top;       //size-当前堆栈的大小，top-栈顶的位置，由于我们会扩展 stack，所以不要把 top 用指针形式存储（?）
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {          //
    void* ret;
    assert(size > 0);                                                   
    if (c->top + size >= c->size) {         //初始c->top为0，堆栈单元size的值为size_t * sizeof(char)
        if (c->size == 0)                   //初始c->size为0，给它赋值堆栈的初始总大小为256size_t
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)    //当堆栈大小不满足，即堆栈过小时，拓充堆栈大小，拓展为原来大小的1.5倍（ 1 + 1/2 ）
            c->size += c->size >> 1;
        c->stack = (char*)realloc(c->stack, c->size);   //c->stack初始时为空指针，第一次执行语句等于realloc(NULL, c->size),分配一个内存块，单位是size_t
        
        /* 为解决内存泄漏的自检方法，暂时不用 */
        //char* new_c_stack = (char*)realloc(c->stack, c->size);
        //if (!new_c_stack) {
        //    puts("Memory realloc error.");
        //    return (void*)LEPT_REALLOC_ERROR;
        //}
        //c->stack = new_c_stack;
        
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char * literal, lept_type type) {       /*把解析非法字符（返回值:LEPT_PARSE_INVALID_VALUE）的部分也包含在literal里*/
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++) {
        if (c->json[i] != literal[i + 1]) {
            return LEPT_PARSE_INVALID_VALUE;
        }
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {      /* 把解析非法字符（返回值:LEPT_PARSE_INVALID_VALUE）的部分也包含在解析数字函数里 */
    const char* p = c->json;    /* p is position */
    /*负号...*/
    if (*p == '-') p++;
    /*整数...*/
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    /*小数点, 小数...*/
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    /*指数...*/
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;

    v->u.n = strtod(c->json, NULL);
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

static int lept_parse_string(lept_context* c, lept_value* v) {              //c里有待解析的json,且c的内容初始化过，v的type初始化为LEPT_NULL
    size_t head = c->top, len;
    const char* p;      //p不能改变它指向的内容，因为p是指向const char的指针
    EXPECT(c, '\"');    //使c->json指向字符串开始字符 '\"' 的下一个字符
    p = c->json;        //使p指向当前跳过'\"'的字符串，不过无法通过操纵p改变字符串内容
    for (;;) {
        char ch = *p++;     //后自增，返回值是自增前的值，运算符有操作数和返回值，要深刻理解这一点
        switch (ch) {
            case '\"' :
                len = c->top - head;
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0' :
                c->top = head;
                return LEPT_PARSE_MISS_QUOTATION_MARK;
            case '\\' :
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case 'n' : PUTC(c, '\n'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/' : PUTC(c, '/');  break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default : 
                        c->top = head;
                        return LEPT_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            default:
                if ((unsigned char)ch < 0x20) {
                    c->top = head;
                    return LEPT_PARSE_INVALID_STRING_CHAR;
                }

                PUTC(c, ch);

        }

    }

}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return lept_parse_number(c, v);   
        case '\"': return lept_parse_string(c, v);
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);          
    c.json = json;
    c.stack = NULL;             //初始化动态堆栈，c.stack初始化为NULL指针，c.size与c.top初始为0
    c.size = c.top = 0;
    lept_init(v);               //待补足后，为什么重复初始化v->type
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK)       //这里别忘了加括号, 否则赋值表达式会出现异常
    {
        lept_parse_whitespace(&c);
        if (*c.json != '\0')
        {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }

    assert(c.top == 0);    /* 加入断言，确保所有数据被弹出 */
    free(c.stack);         /* 释放堆栈内存 */
    return ret;
}

/*如果v->type为LEPT_STRING,则free分配的内存，否则就只完成v->type = LEPT_NULL*/
void lept_free(lept_value* v) {         /* 未完善，之后还要加上对数组和对象的释放 */ 
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_FALSE || v->type == LEPT_TRUE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

int lept_set_string(lept_value* v, const char* s, size_t len) {         //原来返回值是void，为了检测malloc失败的错误并返回，将其改为int类型，并修改.h对应的声明
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    if (v->u.s.s == NULL) {
        puts("Memory allocation error.");
        return LEPT_MALLOC_ERROR;
    }
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;

    return 0;
}
