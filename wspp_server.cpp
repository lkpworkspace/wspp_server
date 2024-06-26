/****************************************************************************
Copyright (c) 2023, likepeng
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
#include "wspp_server_impl.h"

class WsppServer : public myframe::Actor {
 public:
  int Init(const char* param) override {
    // load config
    LoadConfig();
    // register recv handle
    web_.SetRecvHandle(
      std::bind(
        &WsppServer::OnSend,
        this,
        std::placeholders::_1,
        std::placeholders::_2));
    // web_.SetRecvHttpHandle(
    //   std::bind(
    //     &WsppServer::OnHttp,
    //     this,
    //     std::placeholders::_1));
    web_.SetRecvHttpHandle2(
      std::bind(
        &WsppServer::OnHttp2,
        this,
        std::placeholders::_1));
    // listen
    if (web_.Connect()) {
      return -1;
    }
    return 0;
  }

  void Proc(const std::shared_ptr<const myframe::Msg>& msg) override {
    // 处理订阅消息
    if (msg->GetType() == "SUBSCRIBE") {
      LOG(INFO) << "recv SUBSCRIBE from " << msg->GetSrc();
      out_list_.push_back(msg->GetSrc());
      return;
    }
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
    for (int i = 0; i < out_list_.size(); ++i) {
      auto msg = std::make_shared<myframe::Msg>();
      msg->SetDst(out_list_[i]);
      msg->SetData(data);
      msg->SetType("WEBSOCKET");
      msg->SetAnyData(src);
      app->Send(msg);
    }
  }

  std::string OnHttp(const std::string& req) {
    auto app = GetApp();
    if (app == nullptr) {
      LOG(ERROR) << "app is nullptr";
      return "";
    }
    // FIXME: 仅支持第一个订阅用户处理http消息
    for (int i = 0; i < out_list_.size(); ++i) {
      auto msg = std::make_shared<myframe::Msg>();
      msg->SetDst(out_list_[i]);
      msg->SetData(req);
      msg->SetType("HTTP");
      auto resp_msg = app->SendRequest(msg);
      return resp_msg->GetData();
    }
    return "";
  }

  std::shared_ptr<myframe::HttpResp> OnHttp2(const std::shared_ptr<myframe::HttpReq>& req) {
    auto app = GetApp();
    if (app == nullptr) {
      LOG(ERROR) << "app is nullptr";
      return nullptr;
    }
    // FIXME: 仅支持第一个订阅用户处理http消息
    for (int i = 0; i < out_list_.size(); ++i) {
      auto msg = std::make_shared<myframe::Msg>();
      msg->SetDst(out_list_[i]);
      msg->SetAnyData(req);
      msg->SetType("HTTP");
      auto resp_msg = app->SendRequest(msg);
      std::shared_ptr<myframe::HttpResp> resp_pb = nullptr;
      try {
        resp_pb = resp_msg->GetAnyData<std::shared_ptr<myframe::HttpResp>>();
      } catch (std::exception& e) {
        LOG(ERROR) << "convert resp_msg failed, " << e.what();
        return nullptr;
      }
      return resp_pb;
    }
    return nullptr;
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
  }

 private:
  std::vector<std::string> out_list_;
  myframe::WsppServerImpl web_;
};

extern "C" std::shared_ptr<myframe::Actor> actor_create(
  const std::string& actor_name) {
  if (actor_name == "wspp_server") {
    return std::make_shared<WsppServer>();
  }
  return nullptr;
}
