#pragma once

#include <napi.h>
#include "Parser.h"
#include "parserFromKSY.h"

class ParserWrap : public Napi::ObjectWrap<ParserWrap> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Parser", {
    InstanceMethod("addFileStream", &ParserWrap::addFileStream),
    InstanceMethod("getObject", &ParserWrap::getObject),
    InstanceMethod("getType", &ParserWrap::getType),
    InstanceMethod("write", &ParserWrap::write),
      });
  }

  ParserWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<ParserWrap>(info)
  {
    m_Wrappee = parserFromKSY(info[0].ToString().Utf8Value().c_str());
    // m_Wrappee->addFileStream
    // m_Wrappee->getObject()
    // m_Wrappee->getType
    // m_Wrappee->write
  }

private:

  Napi::Value addFileStream(const Napi::CallbackInfo& info) {
    m_Wrappee->addFileStream(info[0].ToString().Utf8Value().c_str());
    return info.Env().Undefined();
  }

  Napi::Value getObject(const Napi::CallbackInfo& info) {
    return info.Env().Undefined();
  }

  Napi::Value getType(const Napi::CallbackInfo& info) {
    return info.Env().Undefined();

  }

  Napi::Value write(const Napi::CallbackInfo& info) {
    return info.Env().Undefined();
  }

private:

  std::shared_ptr<Parser> m_Wrappee;

};
