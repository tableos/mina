#include "stt_whisper.h"

#include <napi.h>

/**
 * Adapted from example
 * https://github.com/nodejs/node-addon-api/blob/main/doc/class_property_descriptor.md
 */
class Adapter : public Napi::ObjectWrap<Adapter>
{
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  Adapter(const Napi::CallbackInfo& info);

 private:
  RealtimeSttWhisper* instance;
  Napi::Value AddAudioData(const Napi::CallbackInfo& info);
  Napi::Value GetTranscribed(const Napi::CallbackInfo& info);
  void Destroy(const Napi::CallbackInfo& info);
};

Napi::Object Adapter::Init(Napi::Env env, Napi::Object exports)
{
  Napi::Function func = DefineClass(
      env,
      "RealtimeSttWhisper",
      {InstanceMethod<&Adapter::AddAudioData>("addAudioData"),
       InstanceMethod<&Adapter::GetTranscribed>("getTranscribed"),
       InstanceMethod<&Adapter::Destroy>("destroy")});

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);
  exports.Set("RealtimeSttWhisper", func);

  return exports;
}

Adapter::Adapter(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Adapter>(info)
{
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::Error::New(info.Env(), "Expected a string specifying path to model")
        .ThrowAsJavaScriptException();
    throw -1;
  }

  Napi::String path_model = info[0].As<Napi::String>();
  instance = new RealtimeSttWhisper(path_model);
}

/** Return 0 if succeed, 1 if failed. */
Napi::Value Adapter::AddAudioData(const Napi::CallbackInfo& info)
{
  // Example of typed array
  // https://github.com/nodejs/node-addon-examples/blob/main/typed_array_to_native/node-addon-api/typed_array_to_native.cc
  if (info.Length() < 1 || !info[0].IsTypedArray()) {
    Napi::Error::New(info.Env(), "Expected a TypedArray")
        .ThrowAsJavaScriptException();
    return Napi::Number::New(info.Env(), 1);
  }

  Napi::TypedArray typedArray = info[0].As<Napi::TypedArray>();

  if (typedArray.TypedArrayType() != napi_float32_array) {
    Napi::Error::New(info.Env(), "Expected a Float32Array")
        .ThrowAsJavaScriptException();
    return Napi::Number::New(info.Env(), 1);
  }

  Napi::Float32Array float32Array = typedArray.As<Napi::Float32Array>();

  std::vector<float> new_data(
      float32Array.Data(),
      float32Array.Data() + float32Array.ElementLength());

  instance->AddAudioData(new_data);

  return Napi::Number::New(info.Env(), 0);
}

Napi::Value Adapter::GetTranscribed(const Napi::CallbackInfo& info)
{
  std::vector<transcribed_msg> msgs;
  msgs = instance->GetTranscribed();
  
  Napi::Env env = info.Env();
  Napi::Array js_msgs = Napi::Array::New(env, msgs.size());

  for (int i = 0; i < (int) msgs.size(); i++) {
    transcribed_msg msg = msgs[i];
    Napi::Object js_msg = Napi::Object::New(env);
    js_msg.Set("text", msg.text);
    js_msg.Set("isPartial", msg.is_partial);
    js_msgs.Set(i, js_msg);
  }

  Napi::Object js_all = Napi::Object::New(env);
  js_all.Set("msgs", js_msgs);

  return js_all;
}

/**
 * JS GC can't work with C++ destructor, so here's a method that can be
 * called manually to stop stt and free resources.
 */
void Adapter::Destroy(const Napi::CallbackInfo& info)
{
  instance->~RealtimeSttWhisper();
}

// Initialize native add-on
Napi::Object Init(Napi::Env env, Napi::Object exports)
{
  Adapter::Init(env, exports);
  return exports;
}

// Register and initialize native add-on
NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)