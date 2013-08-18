#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "i2c-dev.h"

int fd;
int8_t addr;

void setAddress(int8_t addr) {
  int result = ioctl(fd, I2C_SLAVE_FORCE, addr);
  if (result == -1) {
    v8::ThrowException(
      v8::Exception::TypeError(v8::String::New("Failed to set address"))
    );
  }
}

void SetAddress(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  addr = args[0]->Int32Value();
  setAddress(addr);
}

void Scan(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  int i, res;
  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[0]);
  v8::Local<v8::Array> results(v8::Array::New(128));
  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());

  for (i = 0; i < 128; i++) {
    setAddress(i);
    if ((i >= 0x30 && i <= 0x37) || (i >= 0x50 && i <= 0x5F)) {
      res = i2c_smbus_read_byte(fd);
    } else {
      res = i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);
    }
    if (res >= 0) {
      res = i;
    }
    results->Set(i, v8::Integer::New(res));
  }

  setAddress(addr);

  const unsigned argc = 2;
  v8::Local<v8::Value> argv[argc] = { err, results };
  callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);

  args.GetReturnValue().Set(results);
}

void Close(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  if (fd > 0) {
    close(fd);
  }
}

void Open(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  v8::String::Utf8Value device(args[0]);

  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());

  fd = open(*device, O_RDWR);
  if (fd == -1) {
    err = v8::Exception::Error(v8::String::New("Failed to open I2C device"));
  }

  if (args[1]->IsFunction()) {
    const unsigned argc = 1;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
    v8::Local<v8::Value> argv[argc] = { err };

    callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
  }

}

void ReadByte(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  v8::Local<v8::Value> data;
  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());

  int8_t res = i2c_smbus_read_byte(fd);

  if (res == -1) {
    err = v8::Exception::Error(v8::String::New("Cannot read device"));
  } else {
    data = v8::Integer::New(res);
  }

  if (args[0]->IsFunction()) {
    const unsigned argc = 2;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[0]);
    v8::Local<v8::Value> argv[argc] = { err, data };

    callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
  }
  args.GetReturnValue().Set(data);
}

/*
void ReadBlock(const FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());

  int8_t cmd = args[0]->Int32Value();
  int32_t len = args[1]->Int32Value();
  uint8_t data[len];
  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());
  node::Buffer *buffer =  node::Buffer::New(len);

  v8::Local<Object> globalObj = v8::Context::GetCurrent()->Global();
  v8::Local<v8::Function> bufferConstructor = v8::Local<v8::Function>::Cast(globalObj->Get(v8::String::New("Buffer")));
  Handle<v8::Value> constructorArgs[3] = { buffer->handle_, v8::Integer::New(len), v8::Integer::New(0) };
  v8::Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);

  while (fd > 0) {
    if (i2c_smbus_read_i2c_block_data(fd, cmd, len, data) != len) {
      err = v8::Exception::Error(v8::String::New("Error reading length of bytes"));
    }

    memcpy(node::Buffer::Data(buffer), data, len);

    if (args[3]->IsFunction()) {
      const unsigned argc = 2;
      v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[3]);
      v8::Local<v8::Value> argv[argc] = { err, actualBuffer };
      callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
    }

    if (args[2]->IsNumber()) {
      int32_t delay = args[2]->Int32Value();
      usleep(delay * 1000);
    } else {
      break;
    }
  }
  args.GetReturnValue().Set(actualBuffer);
}
*/


void WriteByte(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  int8_t byte = args[0]->Int32Value();
  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());

  if (i2c_smbus_write_byte(fd, byte) == -1) {
    err = v8::Exception::Error(v8::String::New("Cannot write to device"));
  }

  if (args[1]->IsFunction()) {
    const unsigned argc = 1;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
    v8::Local<v8::Value> argv[argc] = { err };

    callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
  }
}

void WriteBlock(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());

  v8::Local<v8::Value> buffer = args[1];

  int8_t cmd = args[0]->Int32Value();
  int   len = node::Buffer::Length(buffer->ToObject());
  char* data = node::Buffer::Data(buffer->ToObject());

  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());

  if (i2c_smbus_write_i2c_block_data(fd, cmd, len, (unsigned char*) data) == -1) {
    err = v8::Exception::Error(v8::String::New("Cannot write to device"));
  }

  if (args[2]->IsFunction()) {
    const unsigned argc = 1;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[2]);
    v8::Local<v8::Value> argv[argc] = { err };

    callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
  }

}

void WriteWord(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  
  int8_t cmd = args[0]->Int32Value();
  int16_t word = args[1]->Int32Value();

  v8::Local<v8::Value> err = v8::Local<v8::Value>::New(v8::Null());
  
  if (i2c_smbus_write_word_data(fd, cmd, word) == -1) {
    err = v8::Exception::Error(v8::String::New("Cannot write to device"));
  }

  if (args[2]->IsFunction()) {
    const unsigned argc = 1;
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[2]);
    v8::Local<v8::Value> argv[argc] = { err };

    callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
  }

}

void Init(v8::Handle<v8::Object> target) {
  target->Set(v8::String::NewSymbol("scan"),
    v8::FunctionTemplate::New(Scan)->GetFunction());

  target->Set(v8::String::NewSymbol("setAddress"),
    v8::FunctionTemplate::New(SetAddress)->GetFunction());

  target->Set(v8::String::NewSymbol("open"),
    v8::FunctionTemplate::New(Open)->GetFunction());

  target->Set(v8::String::NewSymbol("close"),
    v8::FunctionTemplate::New(Close)->GetFunction());

  target->Set(v8::String::NewSymbol("writeByte"),
      v8::FunctionTemplate::New(WriteByte)->GetFunction());

  target->Set(v8::String::NewSymbol("writeBlock"),
      v8::FunctionTemplate::New(WriteBlock)->GetFunction());

  target->Set(v8::String::NewSymbol("readByte"),
    v8::FunctionTemplate::New(ReadByte)->GetFunction());

//  target->Set(v8::String::NewSymbol("readBlock"),
//    v8::FunctionTemplate::New(ReadBlock)->GetFunction());
//
}

NODE_MODULE(i2c, Init)