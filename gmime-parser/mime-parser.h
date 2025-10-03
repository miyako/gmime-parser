#ifndef __MIME_PARSER_H__
#define __MIME_PARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <locale>
#include <fstream>

#include <json/json.h>
#include <sstream>
#include <iostream>

struct Document {
    std::string type;
    Json::Value headers;
    std::string subject;
    std::string body;
    std::string from;
};

#ifdef _WIN32
#define NOMINMAX
#define _SSIZE_T_DEFINED 1
#include <Windows.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define ssize_t __int64
#endif

#include "gmime/gmime.h"
#include "gmime/gmime-parser.h"

#include <tidy.h>
#include <tidybuffio.h>

typedef struct
{
    int level;
    bool is_main_message;
    Document *document;
}mime_ctx;


#define BUFLEN 4096

#ifdef __GNUC__
#define _fopen fopen
#define _fseek fseek
#define _ftell ftell
#define _rb "rb"
#define _wb "wb"
#else
#define _fopen _wfopen
#define _fseek _fseeki64
#define _ftell _ftelli64
#define _rb L"rb"
#define _wb L"wb"
#endif

#ifdef __GNUC__
#define OPTARG_T char*
#include <getopt.h>
#else
#ifndef _WINGETOPT_H_
#define _WINGETOPT_H_
#define OPTARG_T wchar_t*
#define main wmain
#define NULL    0
#define EOF    (-1)
#define ERR(s, c)    if(opterr){\
char errbuf[2];\
errbuf[0] = c; errbuf[1] = '\n';\
fputws(argv[0], stderr);\
fputws(s, stderr);\
fputwc(c, stderr);}
#ifdef __cplusplus
extern "C" {
#endif
    extern int opterr;
    extern int optind;
    extern int optopt;
    extern OPTARG_T optarg;
    extern int getopt(int argc, OPTARG_T *argv, OPTARG_T opts);
#ifdef __cplusplus
}
#endif
#endif  /* _WINGETOPT_H_ */
#endif

#endif  /* __MIME_PARSER_H__ */
