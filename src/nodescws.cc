#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include <nan.h>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include "nodescws.h"
#include "libscws/scws.h"

#define RESMEMSTEP 500
#define MAXDIRLEN 1024
static int debug = 0;

using namespace v8;
using std::string;

Nodescws::Nodescws(){};
Nodescws::~Nodescws(){};

static void scws_log(int level, const char *msg, ...)
{
        if (!debug) return;
        va_list msg_args;
        va_start(msg_args, msg);
        switch (level)
        {
                case NODESCWS_MSG_ERR:
                        printf("[scws ERROR] ");
                        break;
                case NODESCWS_MSG_WARNING:
                        printf("[scws WARNING] ");
                        break;
                case NODESCWS_MSG_LOG:
                        printf("[scws LOG] ");
                        break;
        }
        vprintf(msg, msg_args);
        va_end(msg_args);
}

void Nodescws::Init(Handle<Object> target)
{
        NanScope();

        // Prepare constructor template
        Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
        tpl->SetClassName(NanNew("scws"));
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        // Prototype
        tpl->PrototypeTemplate()->Set(NanNew("segment"),
                NanNew<FunctionTemplate>(Segment)->GetFunction());
        tpl->PrototypeTemplate()->Set(NanNew("destroy"),
                NanNew<FunctionTemplate>(Destroy)->GetFunction());
        
        Persistent<Function> constructor;
        NanAssignPersistent    (constructor, tpl->GetFunction());
        target->Set(NanNew("init"), tpl->GetFunction());
}

NAN_METHOD(Nodescws::New)
{
        NanScope();

        Nodescws *nodescws = new Nodescws();

        if (!args[0]->IsObject()) {
                NanThrowTypeError("[scws ERROR] init argument \
                        should be an object to specify configurations");
                NanReturnUndefined();
        }

        // init scws
        scws_t scws = scws_new();
        Local<Object> Settings = args[0]->ToObject();

        // setup debug mode
        Local<Boolean> Debug = Settings->Get(NanNew("debug"))->ToBoolean();
        if (Debug->BooleanValue())
                debug = 1;

        // setup charset
        std::string Charset(*NanUtf8String(Settings->Get(NanNew("charset"))));
        string charset = Charset.c_str();
        if (charset ==  "undefined") {
                scws_log(NODESCWS_MSG_WARNING, "Charset not specified\n");
                charset = "utf8";
        }
        else if (charset == "utf8" && charset == "gbk")
                charset = "utf8";
        scws_log(NODESCWS_MSG_LOG, "Setting charset: %s\n", charset.c_str());
        scws_set_charset(scws, charset.c_str());

        // setup dict
        std::string Dicts(*NanUtf8String(Settings->Get(NanNew("dicts"))));
        char *dicts = (char *)Dicts.c_str();
        if (strcmp(dicts, "undefined") == 0) {
                scws_log(NODESCWS_MSG_WARNING, "Dict not specified, loading from the default path\n");
                if(scws_add_dict(scws, "./node_modules/scws/dicts/dict.utf8.xdb", SCWS_XDICT_XDB) == -1)
                        scws_log(NODESCWS_MSG_ERR, "Default dict not loaded\n");
        }
        else {
                int dict_mode;
                if (strchr(dicts, ':') != NULL) {
                        while (*dicts != '\0') {
                                char *dict = (char *)malloc(sizeof(char) * MAXDIRLEN);
                                int i = 0;
                                while (i < MAXDIRLEN && *dicts != ':' && *dicts != '\0')
                                        dict[i++] = *dicts++;
                                dict[i++] = '\0';
                                if (*dicts != '\0')
                                        dicts++; // skip the ':'

                                if (strstr(dict, ".txt") != NULL)
                                        dict_mode = SCWS_XDICT_TXT;
                                else
                                        dict_mode = SCWS_XDICT_XDB;
                                scws_log(NODESCWS_MSG_LOG, "Setting dict: %s\n", dict);
                                if (scws_add_dict(scws, dict, dict_mode) == -1)
                                        scws_log(NODESCWS_MSG_ERR, "Failed to load dict %s\n", dict);
                                free(dict);
                        }
                }
                else {
                        if (strstr(dicts, ".txt") != NULL)
                                dict_mode = SCWS_XDICT_TXT;
                        else
                                dict_mode = SCWS_XDICT_XDB;
                        scws_log(NODESCWS_MSG_LOG, "Setting dict: %s\n", dicts);
                        if (scws_add_dict(scws, dicts, dict_mode) == -1) {
                                scws_log(NODESCWS_MSG_ERR, "Failed to load dict %s\n", dicts);
                        }
                }
        }

        // set rules
        std::string Rule(*NanUtf8String(Settings->Get(NanNew("rule"))));
        char *rule = (char *)Rule.c_str();
        if (strcmp(rule, "undefined") == 0) {
                scws_log(NODESCWS_MSG_WARNING, "Rule not specified, loading from the default path\n");
                scws_set_rule(scws, "./node_modules/scws/rules/rules.utf8.ini");
        }
        // json
        else if (strstr(rule, ".json") != NULL) {
                scws_log(NODESCWS_MSG_LOG, "Setting specified JSON rule %s\n", rule);
                scws_set_rule_json(scws, rule, SCWS_RULE_JSON_FILE);
        }
        // ini
        else if (strstr(rule, ".ini") > 0) {
                scws_log(NODESCWS_MSG_LOG, "Setting specified rule %s\n", rule);
                scws_set_rule(scws, rule);
        }
        // JSON string
        else {
                scws_log(NODESCWS_MSG_LOG, "Setting specified JSON string rule %s\n", rule);
                scws_set_rule_json(scws, rule, SCWS_RULE_JSON_STRING);
        }

        // set ignore punctuation
        Local<Boolean> IgnorePunct = Settings->Get(NanNew("ignorePunct"))->ToBoolean();
        if (IgnorePunct->BooleanValue())
                scws_set_ignore(scws, 1);

        Local<Boolean> ApplyStopWord = Settings->Get(NanNew("applyStopWord"))->ToBoolean();
        if (ApplyStopWord->BooleanValue()) {
                scws_set_stopword(scws, 1);
                scws_log(NODESCWS_MSG_LOG, "Set apply stop word\n");
        }
        else {
                scws_set_stopword(scws, 0);
                scws_log(NODESCWS_MSG_LOG, "Set not to apply stop word\n");
        }

        std::string Multi(*NanUtf8String(Settings->Get(NanNew("multi"))));
        char *multi = (char *)Multi.c_str();
        if (strcmp(multi, "undefined") == 0)
                scws_log(NODESCWS_MSG_WARNING, "Multi mode not set, using default\n");
        else {
                int multi_mode = SCWS_MULTI_SHORT;
                if (strcmp(multi, "short") == 0)
                        multi_mode = SCWS_MULTI_SHORT;
                else if(strcmp(multi, "duality") == 0)
                        multi_mode = SCWS_MULTI_DUALITY;
                else if(strcmp(multi, "zmain") == 0)
                        multi_mode = SCWS_MULTI_ZMAIN;
                else if(strcmp(multi, "zall") == 0)
                        multi_mode = SCWS_MULTI_ZALL;
                scws_log(NODESCWS_MSG_LOG, "Setting multi mode %s\n", multi);
                scws_set_multi(scws, multi_mode);
        }

        // scws->InstanceTemplate()->SetInternalField(0, scws);
        nodescws->scws = scws;
        nodescws->Wrap(args.This());

        NanReturnValue(args.This());
}


NAN_METHOD(Nodescws::Segment)
{
        NanScope();

        if (!args[0]->IsString()) {
                NanThrowTypeError("[scws ERROR] segment argument \
                        should be the string to segment");
                NanReturnUndefined();
        }

        Nodescws *nodescws = node::ObjectWrap::Unwrap<Nodescws>(args.This());
        scws_t scws = nodescws->scws;
        std::string Text(*NanUtf8String(args[0]->ToString()));
        char *text = (char *)Text.c_str();
        scws_send_text(scws, text, strlen(text));

        scws_res_t res;
        int result_words_count = 0;
        long memsize = RESMEMSTEP * sizeof(scws_result);
        int memsteps = 1;
        // scws_result *results_raw = (scws_result *)malloc(memsize);
        nodescws->result_raw_ = (scws_result *)malloc(memsize);

        res = scws_get_result(scws);
        while (res != NULL) {
                memcpy(&nodescws->result_raw_[result_words_count], res, sizeof(*res));
                result_words_count++;
                if (result_words_count >= RESMEMSTEP * memsteps) {
                        long new_size = RESMEMSTEP * (memsteps + 1) * sizeof(scws_result);
                        if ((nodescws->result_raw_ = (scws_result *)realloc(nodescws->result_raw_, new_size)) == NULL) {
                                scws_log(NODESCWS_MSG_ERR, "Failed to allocate memory for results\n");
                                free(nodescws->result_raw_);
                                scws_free(scws);
                                NanReturnValue(Array::New(0));
                        }
                        memsteps++;
                }
                res = res->next;
        }
        scws_free_result(res);


        Handle<Array> array = NanNew<Array>(result_words_count);
        for (int i = 0; i < result_words_count; i++) {
                scws_result *cur = &nodescws->result_raw_[i];
                char *str = (char *)malloc(((int)cur->len + 1) * sizeof(char));
                sprintf(str, "%.*s", cur->len, text + cur->off);
                str[(int)cur->len] = '\0';

                Local<Object> obj = NanNew<Object>();
                obj->Set(NanNew<String>("word"), NanNew(str));
                free(str);
                obj->Set(NanNew<String>("offset"), NanNew<Number>(cur->off));
                obj->Set(NanNew<String>("length"), NanNew<Number>(cur->len));
                obj->Set(NanNew<String>("attr"), NanNew(cur->attr));
                obj->Set(NanNew<String>("idf"), NanNew<Number>(cur->idf));
                array->Set(i, obj);
        }

        free(nodescws->result_raw_);
        NanReturnValue(array);
}

NAN_METHOD(Nodescws::Destroy)
{
        NanScope();
        Nodescws *nodescws = node::ObjectWrap::Unwrap<Nodescws>(args.This());
        scws_free(nodescws->scws);
        NanReturnUndefined();
}
