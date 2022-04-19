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
/* usage : ������չ�ռ䣬Ȼ���chѹջ    */
#define PUTC(c, ch)         do {*(char*)lept_context_push(c, sizeof(char) ) = ch; } while(0)
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {                /*��һ����̬��ջ�����ݷ���lept_context����ṹ��*/
    const char* json;
    char* stack;
    size_t size, top;       //size-��ǰ��ջ�Ĵ�С��top-ջ����λ�ã��������ǻ���չ stack�����Բ�Ҫ�� top ��ָ����ʽ�洢��?��
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {          //
    void* ret;
    assert(size > 0);                                                   
    if (c->top + size >= c->size) {         //��ʼc->topΪ0����ջ��Ԫsize��ֵΪsize_t * sizeof(char)
        if (c->size == 0)                   //��ʼc->sizeΪ0��������ֵ��ջ�ĳ�ʼ�ܴ�СΪ256size_t
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)    //����ջ��С�����㣬����ջ��Сʱ���س��ջ��С����չΪԭ����С��1.5���� 1 + 1/2 ��
            c->size += c->size >> 1;
        c->stack = (char*)realloc(c->stack, c->size);   //c->stack��ʼʱΪ��ָ�룬��һ��ִ��������realloc(NULL, c->size),����һ���ڴ�飬��λ��size_t
        
        /* Ϊ����ڴ�й©���Լ췽������ʱ���� */
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

static int lept_parse_literal(lept_context* c, lept_value* v, const char * literal, lept_type type) {       /*�ѽ����Ƿ��ַ�������ֵ:LEPT_PARSE_INVALID_VALUE���Ĳ���Ҳ������literal��*/
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

static int lept_parse_number(lept_context* c, lept_value* v) {      /* �ѽ����Ƿ��ַ�������ֵ:LEPT_PARSE_INVALID_VALUE���Ĳ���Ҳ�����ڽ������ֺ����� */
    const char* p = c->json;    /* p is position */
    /*����...*/
    if (*p == '-') p++;
    /*����...*/
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    /*С����, С��...*/
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    /*ָ��...*/
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

static int lept_parse_string(lept_context* c, lept_value* v) {              //c���д�������json,��c�����ݳ�ʼ������v��type��ʼ��ΪLEPT_NULL
    size_t head = c->top, len;
    const char* p;      //p���ܸı���ָ������ݣ���Ϊp��ָ��const char��ָ��
    EXPECT(c, '\"');    //ʹc->jsonָ���ַ�����ʼ�ַ� '\"' ����һ���ַ�
    p = c->json;        //ʹpָ��ǰ����'\"'���ַ����������޷�ͨ������p�ı��ַ�������
    for (;;) {
        char ch = *p++;     //������������ֵ������ǰ��ֵ��������в������ͷ���ֵ��Ҫ��������һ��
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
    c.stack = NULL;             //��ʼ����̬��ջ��c.stack��ʼ��ΪNULLָ�룬c.size��c.top��ʼΪ0
    c.size = c.top = 0;
    lept_init(v);               //�������Ϊʲô�ظ���ʼ��v->type
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK)       //��������˼�����, ����ֵ���ʽ������쳣
    {
        lept_parse_whitespace(&c);
        if (*c.json != '\0')
        {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }

    assert(c.top == 0);    /* ������ԣ�ȷ���������ݱ����� */
    free(c.stack);         /* �ͷŶ�ջ�ڴ� */
    return ret;
}

/*���v->typeΪLEPT_STRING,��free������ڴ棬�����ֻ���v->type = LEPT_NULL*/
void lept_free(lept_value* v) {         /* δ���ƣ�֮��Ҫ���϶�����Ͷ�����ͷ� */ 
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

int lept_set_string(lept_value* v, const char* s, size_t len) {         //ԭ������ֵ��void��Ϊ�˼��mallocʧ�ܵĴ��󲢷��أ������Ϊint���ͣ����޸�.h��Ӧ������
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
