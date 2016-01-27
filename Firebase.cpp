//
// Copyright 2015 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "Firebase.h"

const char* firebaseFingerprint = "7A 54 06 9B DC 7A 25 B3 86 8D 66 53 48 2C 0B 96 42 C7 B3 0A";
const uint16_t firebasePort = 443;

Firebase::Firebase(const String& host) : _host(host) {
  _http.setReuse(true);
}

Firebase& Firebase::auth(const String& auth) {
  _auth = auth;
  return *this;
}

FirebaseObject Firebase::create() {
  return FirebaseObject{};
}

FirebaseObject Firebase::get(const String& path) {
  return sendRequest("GET", path);
}

FirebaseObject Firebase::push(const String& path, const FirebaseObject& value) {
  char buffer[256];
  value.printTo(buffer, sizeof(buffer));
  return sendRequest("POST", path, buffer);
}

FirebaseObject Firebase::stream(const String& path) {
  String url = makeURL(path); 
  const char* headers[] = {"Location"};
  _http.setReuse(true);  
  _http.begin(_host.c_str(), firebasePort, url.c_str(), true, firebaseFingerprint);
  _http.collectHeaders(headers, 1);
  _http.addHeader("Accept", "text/event-stream");
  int statusCode = _http.sendRequest("GET", (uint8_t*)NULL, 0);
  String location;
  // TODO(proppy): Add a max redirect check
  while (statusCode == 307) {
    location = _http.header("Location");
    _http.setReuse(false);
    _http.end();
    _http.setReuse(true);    
    _http.begin(location, firebaseFingerprint);
    statusCode = _http.sendRequest("GET", (uint8_t*)NULL, 0);
  }
  if (statusCode != 200) {
    return FirebaseObject(FirebaseError(statusCode,
					"stream " + location + ": "
					+ HTTPClient::errorToString(statusCode)));
  }
  return FirebaseObject{};
}

String Firebase::makeURL(const String& path) {
  String url;
  if (path[0] != '/') {
    url = "/";
  }
  url += path + ".json";
  if (_auth.length() > 0) {
    url += "?auth=" + _auth;
  }
  return url;
}

FirebaseObject Firebase::sendRequest(const char* method, const String& path, const String& value) {
  String url = makeURL(path);
  _http.begin(_host.c_str(), firebasePort, url.c_str(), true, firebaseFingerprint);
  int statusCode = _http.sendRequest(method, (uint8_t*)value.c_str(), value.length());
  if (statusCode < 0) {
    return FirebaseObject(FirebaseError(statusCode,
					String(method) + " " + url + ": "
					+ HTTPClient::errorToString(statusCode)));
  }
  // no _http.end() because of connection reuse.
  return FirebaseObject(_http.getString());
}

bool Firebase::connected() {
  return _http.connected();
}

bool Firebase::available() {
  return _http.getStreamPtr()->available();
}

FirebaseObject Firebase::read() {
  auto client = _http.getStreamPtr();
  String event = client->readStringUntil('\n').substring(7);
  String data = client->readStringUntil('\n').substring(6);
  client->readStringUntil('\n'); // consume separator
  FirebaseObject result(data);
  result["event"] = event;
  return result;
}
