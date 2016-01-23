/*
  Asynchronous TCP library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "AsyncPrinter.h"

AsyncPrinter::AsyncPrinter()
  : _client(NULL)
  , _data_cb(NULL)
  , _data_arg(NULL)
  , _close_cb(NULL)
  , _close_arg(NULL)
  , _tx_buffer(NULL)
  , _tx_buffer_size(0)
  , next(NULL)
{}

AsyncPrinter::AsyncPrinter(AsyncClient *client, size_t txBufLen)
  : _client(client)
  , _data_cb(NULL)
  , _data_arg(NULL)
  , _close_cb(NULL)
  , _close_arg(NULL)
  , _tx_buffer(NULL)
  , _tx_buffer_size(txBufLen)
  , next(NULL)
{
  _attachCallbacks();
  _tx_buffer = new ccbuf(_tx_buffer_size);
}

AsyncPrinter::~AsyncPrinter(){
  _on_close();
}

void AsyncPrinter::onData(ApDataHandler cb, void *arg){
  _data_cb = cb;
  _data_arg = arg;
}

void AsyncPrinter::onClose(ApCloseHandler cb, void *arg){
  _close_cb = cb;
  _close_arg = arg;
}

AsyncPrinter::operator bool(){ return connected(); }

AsyncPrinter & AsyncPrinter::operator=(const AsyncPrinter &other){
  if(_client != NULL){
    _client->abort();
    _client->free();
    _client = NULL;
  }
  _tx_buffer_size = other._tx_buffer_size;
  if(_tx_buffer != NULL){
    ccbuf *b = _tx_buffer;
    _tx_buffer = NULL;
    delete b;
  }
  _tx_buffer = new ccbuf(other._tx_buffer_size);
  _client = other._client;
  _attachCallbacks();
  return *this;
}

size_t AsyncPrinter::write(uint8_t data){
  return write(&data, 1);
}

size_t AsyncPrinter::write(const uint8_t *data, size_t len){
  if(_tx_buffer == NULL || !connected())
    return 0;
  size_t toWrite = 0;
  size_t toSend = len;
  while(_tx_buffer->free() < toSend){
    toWrite = _tx_buffer->free();
    _tx_buffer->write((const char*)data, toWrite);
    while(!_client->canSend())
      delay(0);
    _sendBuffer();
    toSend -= toWrite;
  }
  _tx_buffer->write((const char*)(data+(len - toSend)), toSend);
  if(_client->canSend())
    _sendBuffer();
  return len;
}

bool AsyncPrinter::connected(){
  return (_client != NULL && _client->connected());
}

void AsyncPrinter::close(){
  if(_client != NULL)
    _client->close();
}

size_t AsyncPrinter::_sendBuffer(){
  size_t available = _tx_buffer->available();
  if(!connected() || !_client->canSend() || available == 0)
    return 0;
  size_t sendable = _client->space();
  if(sendable < available)
    available= sendable;
  char *out = new char[available];
  _tx_buffer->read(out, available);
  size_t sent = _client->write(out, available);
  delete out;
  return sent;
}

void AsyncPrinter::_onData(void *data, size_t len){
  if(_data_cb)
    _data_cb(_data_arg, this, (uint8_t*)data, len);
}

void AsyncPrinter::_on_close(){
  if(_client != NULL){
    _client = NULL;
  }
  if(_tx_buffer != NULL){
    ccbuf *b = _tx_buffer;
    _tx_buffer = NULL;
    delete b;
  }
  if(_close_cb)
    _close_cb(_close_arg, this);
}

void AsyncPrinter::_attachCallbacks(){
  _client->onPoll([](void *obj, AsyncClient* c){ ((AsyncPrinter*)(obj))->_sendBuffer(); }, this);
  _client->onAck([](void *obj, AsyncClient* c, size_t len, uint32_t time){ ((AsyncPrinter*)(obj))->_sendBuffer(); }, this);
  _client->onDisconnect([](void *obj, AsyncClient* c){ ((AsyncPrinter*)(obj))->_on_close(); c->free(); delete c; }, this);
  _client->onData([](void *obj, AsyncClient* c, void *data, size_t len){ ((AsyncPrinter*)(obj))->_onData(data, len); }, this);
}
