//
//  main.cpp
//  gmime-parser
//
//  Created by miyako on 2025/10/02.
//

#include "mime-parser.h"

static void usage(void)
{
    fprintf(stderr, "Usage:  gmime-parser -r -i in -o out -\n\n");
    fprintf(stderr, "text extractor for txt documents\n\n");
    fprintf(stderr, " -%c path  : %s\n", 'i' , "document to parse");
    fprintf(stderr, " -%c path  : %s\n", 'o' , "text output (default=stdout)");
    fprintf(stderr, " %c        : %s\n", '-' , "use stdin for input");
    fprintf(stderr, " -%c       : %s\n", 'r' , "raw text output (default=json)");
    exit(1);
}

extern OPTARG_T optarg;
extern int optind, opterr, optopt;

#ifdef _WIN32
OPTARG_T optarg = 0;
int opterr = 1;
int optind = 1;
int optopt = 0;
int getopt(int argc, OPTARG_T *argv, OPTARG_T opts) {

    static int sp = 1;
    register int c;
    register OPTARG_T cp;
    
    if(sp == 1)
        if(optind >= argc ||
             argv[optind][0] != '-' || argv[optind][1] == '\0')
            return(EOF);
        else if(wcscmp(argv[optind], L"--") == NULL) {
            optind++;
            return(EOF);
        }
    optopt = c = argv[optind][sp];
    if(c == ':' || (cp=wcschr(opts, c)) == NULL) {
        ERR(L": illegal option -- ", c);
        if(argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return('?');
    }
    if(*++cp == ':') {
        if(argv[optind][sp+1] != '\0')
            optarg = &argv[optind++][sp+1];
        else if(++optind >= argc) {
            ERR(L": option requires an argument -- ", c);
            sp = 1;
            return('?');
        } else
            optarg = argv[optind++];
        sp = 1;
    } else {
        if(argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }
    return(c);
}
#define ARGS (OPTARG_T)L"i:o:-rh"
#else
#define ARGS "i:o:-rh"
#endif

static void print_text(TidyDoc tdoc, TidyNode tnode, std::string& text) {
    
    for (TidyNode child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
        TidyNodeType ttype = tidyNodeGetType(child);
        if (ttype == TidyNode_Text) {
            TidyBuffer buf;
            tidyBufInit(&buf);
            tidyNodeGetValue(tdoc, child, &buf);
            text += std::string((char*)buf.bp, buf.size);
            tidyBufFree(&buf);
        } else if (ttype == TidyNode_Start) {
            print_text(tdoc, child, text);
        }
    }
}

static void html_to_txt_tidy(std::string& html, std::string& txt) {
    
    TidyDoc tdoc = tidyCreate();
    TidyBuffer errbuf;
    tidyBufInit(&errbuf);
    
    tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
    tidyOptSetBool(tdoc, TidyXmlOut, no);
    tidyOptSetBool(tdoc, TidyForceOutput, yes);
    
    tidyOptSetBool(tdoc, TidyQuiet, yes);
    tidyOptSetBool(tdoc, TidyShowWarnings, no);
    tidySetErrorBuffer(tdoc, &errbuf);

    tidyOptSetValue(tdoc, TidyCustomTags, "blocklevel");
    tidyOptSetValue(tdoc, TidyDoctype, "auto");
    
    tidyOptSetBool(tdoc, TidyMark, no);
    tidyOptSetInt(tdoc, TidyWrapLen, 0);
    tidyOptSetBool(tdoc, TidyDropEmptyElems, yes);
    tidyOptSetBool(tdoc, TidyDropEmptyParas, yes);
    tidyOptSetBool(tdoc, TidyDropPropAttrs, yes);

    tidyOptSetBool(tdoc, TidyIndentContent, no);
    tidyOptSetInt(tdoc, TidyIndentSpaces, 0);

    tidyOptSetBool(tdoc, TidyQuoteAmpersand, no);
    tidyOptSetBool(tdoc, TidyAsciiChars, no);
    tidyOptSetBool(tdoc, TidyPreserveEntities, no);
    tidyOptSetBool(tdoc, TidyNumEntities, yes);
    
    if(tidyParseString(tdoc, html.c_str()) >= 0) {
        if(tidyCleanAndRepair(tdoc) >= 0) {
            TidyNode body = tidyGetBody(tdoc);
            print_text(tdoc, body, txt);
        }
    }
    
    tidyRelease(tdoc);
    tidyBufFree(&errbuf);
}

static void processMessage(GMimeObject *parent, GMimeObject *part, gpointer user_data);

/*
static void processAttachmentMessage(GMimeObject *parent, GMimeObject *part, mime_ctx *ctx);

static void processBottomLevel(GMimeObject *parent, GMimeObject *part, gpointer user_data);

static void processNextLevel(GMimeObject *parent, GMimeObject *part, gpointer user_data);
*/

static void processPart(GMimeObject *parent, GMimeObject *part, gpointer user_data) {
    
    mime_ctx *ctx = (mime_ctx *)user_data;
        
    if(GMIME_IS_MESSAGE_PART(part))
    {
        GMimeMessage *message = g_mime_message_part_get_message ((GMimeMessagePart *)part);
                 
//        processAttachmentMessage(parent, part, ctx);
                
        if (message) {
            bool is_main_message = ctx->is_main_message;
            ctx->is_main_message = false;
            ctx->level++;
            g_mime_message_foreach(message, processMessage, ctx);
            ctx->level--;
            ctx->is_main_message = is_main_message;
        }
    }
}

static bool isPartialTextPart(GMimeObject *part) {
    
    if(0 == strncasecmp(part->content_type->type, "message", 7)) {
        if(0 == strncasecmp(part->content_type->subtype, "partial", 7)) {
            return true;
        }
    }
    return false;
}

static bool getHeaders(GMimeObject *part, const char *label, Json::Value& json_message) {
    
    bool hasHeaders = false;
    
    GMimeHeaderList *headers = g_mime_object_get_header_list (part);
    
    if(headers)
    {
        int len = g_mime_header_list_get_count(headers);
        
        if(len)
        {
            hasHeaders = true;
            
            Json::Value header_array = Json::Value(Json::arrayValue);
                        
            for(int i = 0; i < len; ++i)
            {
                GMimeHeader *h = g_mime_header_list_get_header_at(headers, i);
                
                Json::Value item = Json::Value(Json::objectValue);
                            
                item["name"]  = g_mime_header_get_name(h);
                item["value"] = g_mime_utils_header_decode_text(NULL, g_mime_header_get_value(h));
                
                header_array.append(item);
            }
            json_message[label] = header_array;
        }
        g_mime_header_list_clear(headers);
    }
    return hasHeaders;
}

static void processBodyOrAttachment(GMimeObject *parent, GMimeObject *part, mime_ctx *ctx) {
    
    //wrapper exists for part except message/rfc822
    GMimeDataWrapper *wrapper = g_mime_part_get_content((GMimePart *)part);
    
    if(wrapper)
    {
        GMimeContentType *partMediaType = g_mime_object_get_content_type(part);
        const char *mediaType = g_mime_content_type_get_media_type(partMediaType);
        const char *mediaSubType = g_mime_content_type_get_media_subtype (partMediaType);
        
        if(0 == strncasecmp(mediaType, "text", 4))
        {
            //g_mime_text_part_get_text will alloc
            // https://developer.gnome.org/gmime/stable/GMimeTextPart.html
            
            //special consideration for microsoft mht
            const char *charset = g_mime_text_part_get_charset((GMimeTextPart *)part);
            if(charset && (0 == strncasecmp(charset, "unicode", 7)))
            {
                g_mime_text_part_set_charset ((GMimeTextPart *)part, "utf-16le");
            }
            if(charset) {
                char *text = g_mime_text_part_get_text((GMimeTextPart *)part);
                if(text) {
                    if(0 == strncasecmp(mediaSubType, "html", 4))
                    {
                        std::string html = text;
                        std::string plain;
                        html_to_txt_tidy  (html, plain);
                        ctx->document->body += plain;
                    }else{
                        ctx->document->body += text;
                    }
                    g_free(text);
                }
            }
        }
    }
}

static void addToBody(GMimeObject *parent, GMimeObject *part, mime_ctx *ctx) {
    
//    ctx->name =  "body";

    processBodyOrAttachment(parent, part, ctx);
}

static void processMessage(GMimeObject *parent, GMimeObject *part, gpointer user_data) {
    
    mime_ctx *ctx = (mime_ctx *)user_data;
    
    if(GMIME_IS_MULTIPART(part)) {
        ctx->level++;
        g_mime_multipart_foreach((GMimeMultipart *)part, processPart, ctx);
        ctx->level--;
    }
    
    bool addedToBody = false;
    
    GMimeContentDisposition *disposition = g_mime_object_get_content_disposition (part);
    
    if    ((GMIME_IS_TEXT_PART(part) && (!disposition))
       || (isPartialTextPart(part))
       || ((GMIME_IS_TEXT_PART(part) && ( disposition) && (0 == strncasecmp(g_mime_content_disposition_get_disposition(disposition), "inline", 6)))))
    {
        if(ctx->is_main_message) {
            addToBody(parent, part, ctx);
            addedToBody = true;
        }
    }
    
    if((!addedToBody) && GMIME_IS_PART(part))
    {
        if(ctx->is_main_message) {
//            addToAttachment(parent, part, ctx);
        }else{
//            addToPart(parent, part, ctx);
        }
    }
    
}

static void document_to_json(Document& document, std::string& text, bool rawText) {
    
    if(rawText){
        text  = document.subject;
        (text.length() != 0) {
            text += "\n";
        }
        text += document.body;
    }else{
        Json::Value documentNode(Json::objectValue);
        documentNode["type"] = document.type;
        documentNode["subject"] = document.subject;
        documentNode["body"] = document.body;
        
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        text = Json::writeString(writer, documentNode);
    }
}

static void getAddress(InternetAddressList *list, const char *label, Json::Value& json_message) {
    
    InternetAddress *address;
    
    if(list)
    {
        int len = internet_address_list_length(list);
        
        if(len)
        {
            Json::Value address_array = Json::Value(Json::arrayValue);
            
            for(int i = 0; i < len; ++i)
            {
                address = internet_address_list_get_address(list, i);
                
                Json::Value item = Json::Value(Json::objectValue);
                
                //internet_address_to_string will alloc
                // https://developer.gnome.org/gmime/stable/InternetAddress.html#internet-address-to-string
                
                char *string = internet_address_to_string(address, NULL, FALSE);
                item["string"] = string ? string : "";
                if(string) g_free(string);
                
                char *encoded_string = internet_address_to_string(address, NULL, TRUE);
                item["encoded_string"] = encoded_string ? encoded_string : "";
                if(encoded_string) g_free(encoded_string);
                
                const char *addr = internet_address_mailbox_get_addr((InternetAddressMailbox *)address);
                item["addr"] = addr ? addr : "";

                const char *idn_addr = internet_address_mailbox_get_idn_addr((InternetAddressMailbox *)address);
                item["idn_addr"] = idn_addr ? idn_addr : "";
                
                const char *name = internet_address_get_name(address);
                item["name"] = name ? name : "";
                
                address_array.append(item);
                
            }
            json_message[label] = address_array;

        }else
        {
            //create an empty array
            json_message[label] = Json::Value(Json::arrayValue);
        }
        internet_address_list_clear(list);
    }else
    {
        //create an empty array
        json_message[label] = Json::Value(Json::arrayValue);
    }
    
}

int main(int argc, OPTARG_T argv[]) {
    
    const OPTARG_T input_path  = NULL;
    const OPTARG_T output_path = NULL;
    
    std::vector<uint8_t>eml_data(0);
    
    int ch;
    std::string text;
    bool rawText = false;
    
    while ((ch = getopt(argc, argv, ARGS)) != -1){
        switch (ch){
            case 'i':
                input_path  = optarg;
                break;
            case 'o':
                output_path = optarg;
                break;
            case '-':
            {
                std::vector<uint8_t> buf(BUFLEN);
                size_t n;
                
                while ((n = fread(buf.data(), 1, buf.size(), stdin)) > 0) {
                    eml_data.insert(eml_data.end(), buf.begin(), buf.begin() + n);
                }
            }
                break;
            case 'r':
                rawText = true;
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }
    
    if((!eml_data.size()) && (input_path != NULL)) {
        FILE *f = _fopen(input_path, _rb);
        if(f) {
            _fseek(f, 0, SEEK_END);
            size_t len = (size_t)_ftell(f);
            _fseek(f, 0, SEEK_SET);
            eml_data.resize(len);
            fread(eml_data.data(), 1, eml_data.size(), f);
            fclose(f);
        }
    }
    
    if(!eml_data.size()) {
        usage();
    }
    
    Document document;
    document.type = "eml";
    
    g_mime_init();
    
    GMimeStream *stream = g_mime_stream_mem_new_with_buffer((const char *)eml_data.data(), eml_data.size());
    if(stream) {
        
        GMimeParserOptions *options = g_mime_parser_options_new();
        g_mime_parser_options_set_address_compliance_mode(options, GMIME_RFC_COMPLIANCE_LOOSE);
        g_mime_parser_options_set_allow_addresses_without_domain(options, true);
        g_mime_parser_options_set_parameter_compliance_mode(options, GMIME_RFC_COMPLIANCE_LOOSE);
        g_mime_parser_options_set_rfc2047_compliance_mode(options, GMIME_RFC_COMPLIANCE_LOOSE);
        GMimeParser *parser = g_mime_parser_new_with_stream (stream);
        if(parser) {
            GMimeMessage *message = g_mime_parser_construct_message (parser, options);
            if(message)
            {
                Json::Value json = Json::Value(Json::objectValue);
                
//                getAddress(g_mime_message_get_from (message), "from", document.headers);
                
                mime_ctx ctx;
                ctx.level = 1;
                ctx.is_main_message = true;
                ctx.document = &document;
                
                const char *message_subject = g_mime_message_get_subject(message);
                if(message_subject)
                    document.subject = message_subject;
                
                g_mime_message_foreach(message, processMessage, &ctx);
                
                g_clear_object(&message);
            }
            g_clear_object(&parser);
        }
        g_mime_parser_options_free(options);

        g_clear_object(&stream);
    }
    
    document_to_json(document, text, rawText);
    
    if(!output_path) {
        std::cout << text << std::endl;
    }else{
        FILE *f = _fopen(output_path, _wb);
        if(f) {
            fwrite(text.c_str(), 1, text.length(), f);
            fclose(f);
        }
    }

    g_mime_shutdown();
    
    return 0;
}
