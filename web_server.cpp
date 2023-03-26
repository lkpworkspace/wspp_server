/****************************************************************************
Copyright (c) 2018, likepeng
All rights reserved.

Author: likepeng <likepeng0418@163.com>
****************************************************************************/
#include <chrono>
#include <memory>
#include <thread>

#include <glog/logging.h>

#include "myframe/msg.h"
#include "myframe/actor.h"
#include "myframe/app.h"
#include "web_server_impl.h"

class WebServer : public myframe::Actor {
 public:
  int Init(const char* param) override {
    // load config
    LoadConfig();
    // register recv handle
    web_.SetRecvHandle(
      std::bind(
        &WebServer::OnSend,
        this,
        std::placeholders::_1,
        std::placeholders::_2));
    // web_.SetRecvHttpHandle(
    //   std::bind(
    //     &WebServer::OnHttp,
    //     this,
    //     std::placeholders::_1));
    web_.SetRecvHttpHandle2(
      std::bind(
        &WebServer::OnHttp2,
        this,
        std::placeholders::_1));
    // listen
    if (web_.Connect()) {
      return -1;
    }
    return 0;
  }

  void Proc(const std::shared_ptr<const myframe::Msg>& msg) override {
    auto handle = msg->GetAnyData<void*>();
    web_.Send(handle, msg->GetData());
  }

  void OnSend(void* src, const std::string& data) {
    LOG(INFO) << "Get msg: " << data;
    auto app = GetApp();
    if (app == nullptr) {
      LOG(ERROR) << "app is nullptr";
      return;
    }
    auto msg = std::make_shared<myframe::Msg>();
    msg->SetData(data);
    msg->SetType("WEBSOCKET");
    msg->SetAnyData(src);
    app->Send(dst_addr_, msg);
  }

  std::string OnHttp(const std::string& req) {
    auto app = GetApp();
    if (app == nullptr) {
      LOG(ERROR) << "app is nullptr";
      return "";
    }
    auto msg = std::make_shared<myframe::Msg>();
    msg->SetData(req);
    msg->SetType("HTTP");
    auto resp_msg = app->SendRequest(dst_addr_, msg);
    return resp_msg->GetData();
  }

  std::shared_ptr<myframe::HttpResp> OnHttp2(const std::shared_ptr<myframe::HttpReq>& req) {
    auto app = GetApp();
    if (app == nullptr) {
      LOG(ERROR) << "app is nullptr";
      return nullptr;
    }
    auto msg = std::make_shared<myframe::Msg>();
    msg->SetAnyData(req);
    msg->SetType("HTTP");
    auto resp_msg = app->SendRequest(dst_addr_, msg);
    std::shared_ptr<myframe::HttpResp> resp_pb = nullptr;
    try {
      resp_pb = resp_msg->GetAnyData<std::shared_ptr<myframe::HttpResp>>();
    } catch (std::exception& e) {
      LOG(ERROR) << "convert resp_msg failed, " << e.what();
      return nullptr;
    }
    return resp_pb;
  }

  void LoadConfig() {
    auto conf = GetConfig();
    web_.SetOption(
      myframe::NetOption::kServerCa,
      (*conf)["server_ca"].asString());
    web_.SetOption(
      myframe::NetOption::kServerKey,
      (*conf)["server_key"].asString());
    web_.SetOption(
      myframe::NetOption::kServerPort,
      (*conf)["server_port"].asString());
    dst_addr_ = (*conf)["dst_addr"].asString();
  }

 private:
  std::string dst_addr_;
  myframe::WebServerImpl web_;
};

extern "C" std::shared_ptr<myframe::Actor> my_actor_create(
  const std::string& actor_name) {
  if (actor_name == "web_server") {
    return std::make_shared<WebServer>();
  }
  return nullptr;
}
