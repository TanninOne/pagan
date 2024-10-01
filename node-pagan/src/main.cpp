/* TODO:
 *  - evaluation order in expressions is wrong but not consistently? What's going on?
 *  - const_cast in DynObject and iowrap
 *  - memory leak in dumpIndex
 *  - support for nested data streams
 *  - more complex write cases
 *  - is the getObject call in its current form even necessary?
 *  - setList isn't implemented
 *  - further todos, see index_format.md
 */

#include "iowrap.h"
#include "napi.h"
#include "parserFromKSY.h"
#include "TypeSpec.h"
#include <algorithm>
#include "cpptrace/cpptrace.hpp"

class SpecWrap : public Napi::ObjectWrap<SpecWrap> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function constructor = DefineClass(env, "Spec", {
        InstanceMethod<&SpecWrap::getName>("getName"),
      });

    m_New = Napi::Persistent(constructor);

    return exports;
  }

  static Napi::Value New(const Napi::CallbackInfo &info, std::shared_ptr<TypeSpec> spec) {
    return m_New.New({ Napi::External<std::shared_ptr<TypeSpec>>::New(info.Env(), &spec) });
  }

  SpecWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<SpecWrap>(info)
    , m_Spec(*info[0].As<Napi::External<std::shared_ptr<TypeSpec>>>().Data())
  {
  }

  std::shared_ptr<TypeSpec> getValue() const { return m_Spec; }

private:

  Napi::Value getName(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), m_Spec->getName().c_str());
  }

private:
  std::shared_ptr<TypeSpec> m_Spec;
  static Napi::FunctionReference m_New;
};

Napi::FunctionReference SpecWrap::m_New;

class DynObjectWrap;

class TypeSpecWrap {
public:
  TypeSpecWrap(const std::shared_ptr<TypeSpec>& spec);

  const std::map<std::string, std::function<Napi::Value(const Napi::CallbackInfo& info)>> &getters() const {
    return m_Getters;
  }

private:
  std::shared_ptr<TypeSpec> m_Spec;
  std::map<std::string, std::function<Napi::Value(const Napi::CallbackInfo &info)>> m_Getters;
};

class TypeSpecCatalog {
public:
  TypeSpecWrap *spec(const std::shared_ptr<TypeSpec>& spec) {
    auto iter = m_Types.find(spec->getId());
    if (iter == m_Types.end()) {
      m_Types[spec->getId()] = new TypeSpecWrap(spec);
      iter = m_Types.find(spec->getId());
    }
    return iter->second;
  }
private:
  std::map<uint32_t, TypeSpecWrap*> m_Types;
};

static Napi::String toStringG(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), "foobar x");
}

class DynObjectWrap : public Napi::ObjectWrap<DynObjectWrap> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function constructor = DefineClass(env, "DynObject", {
        InstanceMethod<&DynObjectWrap::get>("get"),
        InstanceMethod<&DynObjectWrap::set>("set"),
        InstanceAccessor<&DynObjectWrap::getKeys>("keys")
      });

    m_New = Napi::Persistent(constructor);

    exports.Set("DynObject", constructor);

    return exports;
  }

  static Napi::Value New(const Napi::CallbackInfo& info, std::shared_ptr<DynObject> obj, TypeSpecCatalog *types) {
    Napi::Object objWrap = m_New.New({
      Napi::External<std::shared_ptr<DynObject>>::New(info.Env(), &obj),
      Napi::External<TypeSpecCatalog>::New(info.Env(), types),
    });
    return objWrap;
  }

  static Napi::Value Getter(const Napi::CallbackInfo& info) {
    std::cout << "getter " << info.Length() << std::endl;
  }

  DynObjectWrap(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<DynObjectWrap>(info)
    , m_Value(*info[0].As<Napi::External<std::shared_ptr<DynObject>>>().Data())
    , m_TypeCatalog(info[1].As<Napi::External<TypeSpecCatalog>>().Data())
  {
    TypeSpecWrap* type = m_TypeCatalog->spec(m_Value->getSpec());

    /*
    for (auto kv : type->getters()) {
      Napi::Object obj = info.This().As<Napi::Object>();
      obj.DefineProperty(
        Napi::PropertyDescriptor::Accessor(info.Env(), obj, kv.first.c_str(), kv.second, napi_default, this));
    }
    */

    /*
    for (const auto& key : m_Value->getKeys()) {
      info.This().As<Napi::Object>().DefineProperty(
        Napi::PropertyDescriptor::Accessor<Getter>(key.c_str(), napi_default, this));
    }
    */
  }

  Napi::Value getter(const Napi::CallbackInfo &info, const char *key) {
    return DynObjectWrap::getFromValue(info, m_Value, key, m_TypeCatalog);
  }

  std::shared_ptr<DynObject> value() const {
    return m_Value;
  }

  static Napi::Value getFromValueCustomList(const Napi::CallbackInfo &info, const std::shared_ptr<DynObject> &parent, const char *key, TypeSpecCatalog *catalog) {
    Napi::Array res = Napi::Array::New(info.Env());
    uint32_t idx = 0;

    for (const DynObject& obj : parent->getList<DynObject>(key)) {
      std::shared_ptr<DynObject> tmp(new DynObject(obj));
      Napi::Value wrap = DynObjectWrap::New(info, tmp, catalog);
      res.Set(idx++, wrap);
    }

    return res;
  }

  static Napi::Value getFromValuePOD(const Napi::CallbackInfo& info, const std::shared_ptr<DynObject>& parent, const char* key, TypeSpecCatalog* catalog) {
    Napi::Array res = Napi::Array::New(info.Env());
    uint32_t idx = 0;

    for (std::any val : parent->getList<std::any>(key)) {
      std::string str = flexi_cast<std::string>(val);
      res.Set(idx++, Napi::String::New(info.Env(), str));
    }

    return res;
  }

  static Napi::Value getFromValue(const Napi::CallbackInfo &info, const std::shared_ptr<DynObject> &parent, const char *key, TypeSpecCatalog *catalog) {
    try {
      if (!parent->has(key)) {
        return info.Env().Null();
      }
      const TypeProperty &type = parent->getChildType(key);
      if (type.isList) {
        bool isCustom = parent->isCustom(key);

        if (!isCustom && (parent->getChildType(key).typeId == TypeId::runtime)) {
          // TODO currently the item type is stored with individual items and the array itself is stored
          //   as "runtime" type, so we don't actually know if it's a custom type or not
          try {
            return getFromValueCustomList(info, parent, key, catalog);
          }
          catch (const WrongTypeRequestedError&) {
            isCustom = false;
          }
        }

        if (isCustom) {
          return getFromValueCustomList(info, parent, key, catalog);
        }

        return getFromValuePOD(info, parent, key, catalog);
      }
      else {
        if (parent->isCustom(key)) {
          DynObject obj = parent->get<DynObject>(key);
          return DynObjectWrap::New(
            info,
            std::shared_ptr<DynObject>(new DynObject(obj)),
            catalog
          );
        }
        else {
          uint8_t* propBuffer;
          uint32_t typeId;
          std::vector<std::string> argList;
          std::tie(typeId, propBuffer, argList) = parent->getEffectiveType(key);

          std::shared_ptr<IOWrapper> dataStream = parent->getDataStream();
          std::shared_ptr<IOWrapper> writeStream = parent->getWriteStream();

          return readValue(info.Env(), static_cast<TypeId>(typeId), reinterpret_cast<char*>(propBuffer), dataStream, writeStream);
        }
      }
    }
    catch (const cpptrace::exception& e) {
      e.trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno));
      throw Napi::Error::New(info.Env(), e.what());
    }
    catch (const std::exception& e) {
      throw Napi::Error::New(info.Env(), e.what());
    }
  }

  static Napi::Value getFromValue(const Napi::CallbackInfo& info, const std::shared_ptr<DynObject>& parent, Napi::Array keys, TypeSpecCatalog* catalog) {
    try {
      DynObject cur = *parent;

      // all subobject except the last have to be objects
      for (int i = 0; i < keys.Length() - 1; ++i) {
        cur = cur.get<DynObject>(keys.Get(i).ToString().Utf8Value().c_str());
      }

      return getFromValue(info, std::shared_ptr<DynObject>(new DynObject(cur)), keys.Get(keys.Length() - 1).ToString().Utf8Value().c_str(), catalog);
    }
    catch (const cpptrace::exception& e) {
      e.trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno));
      throw Napi::Error::New(info.Env(), e.what());
    }
    catch (const std::exception& e) {
      throw Napi::Error::New(info.Env(), e.what());
    }
  }

private:

  static Napi::Value readValue(const Napi::Env &env, TypeId type, char* index, std::shared_ptr<IOWrapper>& data, std::shared_ptr<IOWrapper>& write) {
    switch (type) {
    case TypeId::int8: return Napi::Value::From(env, type_read<int8_t>(type, index, data, write, nullptr));
    case TypeId::int16: return Napi::Value::From(env, type_read<int16_t>(type, index, data, write, nullptr));
    case TypeId::int32: return Napi::Value::From(env, type_read<int32_t>(type, index, data, write, nullptr));
    case TypeId::int64: return Napi::Value::From(env, type_read<int64_t>(type, index, data, write, nullptr));
    case TypeId::uint8: return Napi::Value::From(env, type_read<uint8_t>(type, index, data, write, nullptr));
    case TypeId::uint16: return Napi::Value::From(env, type_read<uint16_t>(type, index, data, write, nullptr));
    case TypeId::uint32: return Napi::Value::From(env, type_read<uint32_t>(type, index, data, write, nullptr));
    case TypeId::uint64: return Napi::Value::From(env, type_read<uint64_t>(type, index, data, write, nullptr));
    case TypeId::float32_iee754: return Napi::Value::From(env, type_read<float>(type, index, data, write, nullptr));
    case TypeId::stringz: return Napi::Value::From(env, type_read<std::string>(type, index, data, write, nullptr));
    case TypeId::string: return Napi::Value::From(env, type_read<std::string>(type, index, data, write, nullptr));
    case TypeId::bytes: {
      std::vector<uint8_t> bytes = type_read<std::vector<uint8_t>>(type, index, data, write, nullptr);

      uint8_t* buffer = new uint8_t[bytes.size()];
      memcpy(buffer, bytes.data(), bytes.size());

      return Napi::Buffer<uint8_t>::New(env, buffer, bytes.size(), [](Napi::Env env, uint8_t *buffer) {
        delete[] buffer;
        });
    } break;
    }

    return env.Undefined();
  }

  template <typename T> static T limit(int32_t input) {
    if ((input < std::numeric_limits<T>::min())
        || (input > std::numeric_limits<T>::max())) {
      throw std::runtime_error("invalid value");
    }
    return static_cast<T>(input);
  }

  static void setValue(DynObject *obj, uint32_t type, const std::string &key, Napi::Value value) {
    switch (type) {
    case TypeId::int8: obj->set(key.c_str(), limit<int8_t>(value.ToNumber().Int32Value())); break;
    case TypeId::int16: obj->set(key.c_str(), limit<int16_t>(value.ToNumber().Int32Value())); break;
    case TypeId::int32: obj->set(key.c_str(), limit<int32_t>(value.ToNumber().Int32Value())); break;
    case TypeId::int64: obj->set(key.c_str(), limit<int64_t>(value.ToNumber().Int64Value())); break;
    case TypeId::uint8: obj->set(key.c_str(), limit<uint8_t>(value.ToNumber().Int32Value())); break;
    case TypeId::uint16: obj->set(key.c_str(), limit<uint16_t>(value.ToNumber().Int32Value())); break;
    case TypeId::uint32: obj->set(key.c_str(), limit<uint32_t>(value.ToNumber().Int32Value())); break;
    case TypeId::uint64: obj->set(key.c_str(), limit<uint64_t>(value.ToNumber().Int64Value())); break;
    case TypeId::float32_iee754: obj->set(key.c_str(), value.ToNumber().FloatValue()); break;
    case TypeId::stringz: obj->set(key.c_str(), value.ToString().Utf8Value()); break;
    case TypeId::string: obj->set(key.c_str(), value.ToString().Utf8Value()); break;
    case TypeId::bytes: {
      Napi::Buffer<uint8_t> buf = value.As<Napi::Buffer<uint8_t>>();
      std::vector<uint8_t> bytes(buf.Data(), buf.Data() + buf.Length());
      obj->set(key.c_str(), bytes);
    } break;
    }
  }

  Napi::Value get(const Napi::CallbackInfo& info) {
    if (info.Length() != 1) {
      throw Napi::Error::New(info.Env(), "Usage: get(<key> | [<keys>])");
    }

    if (info[0].IsArray()) {
      Napi::Array arr = info[0].As<Napi::Array>();
      return getFromValue(info, m_Value, arr, m_TypeCatalog);
    }
    else {
      std::string key = info[0].ToString().Utf8Value();
      return getFromValue(info, m_Value, key.c_str(), m_TypeCatalog);
    }
  }

  Napi::Value set(const Napi::CallbackInfo& info) {
    if (info.Length() != 2) {
      throw Napi::Error::New(info.Env(), "Usage: set(<key>, <value>)");
    }
    std::string key = info[0].ToString().Utf8Value();

    uint8_t* propBuffer;
    uint32_t typeId;
    std::vector<std::string> argList;
    std::tie(typeId, propBuffer, argList) = m_Value->getEffectiveType(key.c_str());

    setValue(m_Value.get(), typeId, key, info[1]);

    return info.Env().Undefined();
  }

  Napi::Value getKeys(const Napi::CallbackInfo& info) {
    Napi::Array res = Napi::Array::New(info.Env());
    int i = 0;
    for (const auto& key : m_Value->getKeys()) {
      res.Set(i++, Napi::String::New(info.Env(), key));
    }
    return res;
  }

  Napi::Value toString(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), "foobar");
  }

private:
  std::shared_ptr<DynObject> m_Value;
  TypeSpecCatalog* m_TypeCatalog;
  static Napi::FunctionReference m_New;
};

inline TypeSpecWrap::TypeSpecWrap(const std::shared_ptr<TypeSpec>& spec)
  : m_Spec(spec)
{
  for (auto prop : m_Spec->getProperties()) {
    m_Getters[prop.key] = [prop](const Napi::CallbackInfo& info) -> Napi::Value {
      DynObjectWrap* wrap = static_cast<DynObjectWrap*>(info.Data());
      return wrap->getter(info, prop.key.c_str());
    };
  }
}

Napi::FunctionReference DynObjectWrap::m_New;

class ParserWrap : public Napi::ObjectWrap<ParserWrap> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  ParserWrap(const Napi::CallbackInfo &info);
  static Napi::Value FromKSY(const Napi::CallbackInfo& info) { return info.Env().Undefined();  }

private:
  std::shared_ptr<Parser> m_Wrappee;
  TypeSpecCatalog m_TypeCatalog;

  // void addFileStream(const char *filePath);
  Napi::Value addFileStream(const Napi::CallbackInfo& info) {
    if (info.Length() != 1) {
      throw Napi::Error::New(info.Env(), "Usage: addFileStream(<filePath>)");
    }
    m_Wrappee->addFileStream(info[0].ToString().Utf8Value().c_str());
    return info.Env().Undefined();
  }

  // std::shared_ptr<TypeSpec> getType(const char *name) const;
  Napi::Value getType(const Napi::CallbackInfo& info) {
    if (info.Length() != 1) {
      throw Napi::Error::New(info.Env(), "Usage: getType(<name>)");
    }

    std::string name(info[0].ToString());

    return SpecWrap::New(info, m_Wrappee->getType(name.c_str()));
  }

  Napi::Value dumpIndex(const Napi::CallbackInfo& info) {
    if (info.Length() != 0) {
      throw Napi::Error::New(info.Env(), "Usage: dumpIndex()");
    }

    std::vector<uint8_t> objIndex = m_Wrappee->objectIndex();
    std::vector<uint8_t> arrayIndex = m_Wrappee->arrayIndex();

    uint8_t header[] = {
      'P', 'I', 'D', 'X',
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 
    };
    uint64_t objIndexSize = objIndex.size();
    uint64_t arrIndexSize = arrayIndex.size();
    uint32_t objectCount = m_Wrappee->numObjects();
    uint32_t arrayCount = m_Wrappee->numArrays();
    uint32_t typeCount = m_Wrappee->numTypes();

    std::string typeNames;
    for (int i = TypeId::custom; i < typeCount; ++i) {
      typeNames += std::to_string(i) + m_Wrappee->getTypeById(i) + ';';
    }

    uint32_t typesSize = typeNames.length();

    memcpy(header + 4, reinterpret_cast<uint8_t*>(&objIndexSize), 8);
    memcpy(header + 12, reinterpret_cast<uint8_t*>(&arrIndexSize), 8);
    memcpy(header + 20, reinterpret_cast<uint8_t*>(&objectCount), 4);
    memcpy(header + 24, reinterpret_cast<uint8_t*>(&arrayCount), 4);
    memcpy(header + 28, reinterpret_cast<uint8_t*>(&typesSize), 4);

    size_t combinedSize = sizeof(header) + typesSize + objIndex.size() + arrayIndex.size();
    uint8_t *combined = new uint8_t[combinedSize];
    memcpy(combined, header, sizeof(header));
    memcpy(combined + sizeof(header), &typeNames[0], typesSize);
    memcpy(combined + sizeof(header) + typesSize, &objIndex[0], objIndexSize);
    memcpy(combined + sizeof(header) + typesSize + objIndexSize, &arrayIndex[0], arrIndexSize);

#pragma message("WARNING memory leak!")
    return Napi::Buffer<uint8_t>::New(info.Env(), &combined[0], combinedSize);
  }

  Napi::Value write(const Napi::CallbackInfo &info) {
    if (info.Length() != 2) {
      throw Napi::Error::New(info.Env(), "Usage: write(<filepath>, <object>)");
    }

    std::string filePath = info[0].ToString().Utf8Value();
    Napi::Object obj = info[1].As<Napi::Object>();
    DynObjectWrap* objWrap = DynObjectWrap::Unwrap(obj);
    try {
      m_Wrappee->write(filePath.c_str(), *(objWrap->value().get()));
    }
    catch (const cpptrace::exception& e) {
      e.trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno));
      throw Napi::Error::New(info.Env(), e.what());
    }
    catch (const std::exception& e) {
      throw Napi::Error::New(info.Env(), e.what());
    }
    return info.Env().Undefined();
  }

  // DynObject getObject(const std::shared_ptr<TypeSpec> &spec, size_t offset, DataStreamId dataStream = 0);
  Napi::Value getObject(const Napi::CallbackInfo& info) {
    if (info.Length() < 2) {
      throw Napi::Error::New(info.Env(), "Usage: getObject(<spec>, <offset>[, dataStream])");
    }

    if (!m_Wrappee->hasInputData()) {
      throw Napi::Error::New(info.Env(), "No data loaded");
    }

    Napi::Object obj = info[0].As<Napi::Object>();
    SpecWrap* specWrap = SpecWrap::Unwrap(obj);
    if (specWrap == nullptr) {
      throw Napi::Error::New(info.Env(), "Invalid type specification");
    }
    std::shared_ptr<TypeSpec> spec = specWrap->getValue();
    size_t offset = info[1].ToNumber().Uint32Value();
    DataStreamId dataStream = 0;
    if (info.Length() > 2) {
      dataStream = info[2].ToNumber().Uint32Value();
    }

    try {
      Napi::Env env = info.Env();
      return DynObjectWrap::New(
        info,
        std::shared_ptr<DynObject>(new DynObject(m_Wrappee->getObject(spec, offset, dataStream))),
        &m_TypeCatalog
      );
    }
    catch (const cpptrace::exception& e) {
      e.trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno));
      throw Napi::Error::New(info.Env(), e.what());
    }
    catch (const std::exception& e) {
      throw Napi::Error::New(info.Env(), e.what());
    }
  }
};

/*
  std::vector<DynObject> getList(const std::shared_ptr<TypeSpec> &spec, size_t offset, DataStreamId dataStream = 0);

  template <typename T>
  DynObject createObject(const std::weak_ptr<TypeSpec> &spec, std::initializer_list<T> data);

  std::shared_ptr<TypeSpec> createType(const char *name);
  std::shared_ptr<TypeSpec> createType(const char *name, const std::initializer_list<TypeAttribute> &attributes);
*/

Napi::Object ParserWrap::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(env, "Parser", {
      InstanceMethod<&ParserWrap::addFileStream>("addFileStream"),
      InstanceMethod<&ParserWrap::getObject>("getObject"),
      InstanceMethod<&ParserWrap::write>("write"),
      InstanceMethod<&ParserWrap::getType>("getType"),
      InstanceMethod<&ParserWrap::dumpIndex>("dumpIndex"),
      StaticMethod<&ParserWrap::FromKSY>("FromKSY"),
    });

  // Napi::FunctionReference* constructor = new Napi::FunctionReference();
  // *constructor = Napi::Persistent(func);

  exports.Set("Parser", func);

  return exports;
}

ParserWrap::ParserWrap(const Napi::CallbackInfo& info)
  : Napi::ObjectWrap<ParserWrap>(info) {
  Napi::String value = info[0].As<Napi::String>();
  try {
    m_Wrappee = parserFromKSY(value.Utf8Value().c_str());
  }
  catch (const std::exception& e) {
    Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
  }
}

static Napi::Object parserFromKSYN(const Napi::CallbackInfo &info) {
  auto filePath = info[0].ToString();

  Napi::Object res = Napi::Object::New(info.Env());

  try {
    std::shared_ptr<Parser> parser = parserFromKSY(filePath.Utf8Value().c_str());
  }
  catch (const std::exception& e) {
    Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
  }

  return Napi::Object::New(info.Env());
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "parserFromKSY"), Napi::Function::New(env, parserFromKSYN));

  ParserWrap::Init(env, exports);
  SpecWrap::Init(env, exports);
  DynObjectWrap::Init(env, exports);

  return exports;
}

NODE_API_MODULE(pagan, Init)

