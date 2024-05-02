/****************************************************************************
Copyright (c) 2023, likepeng
All rights reserved.

Author: likepeng <likepeng0418@163.com>
****************************************************************************/
#pragma once
#include <unordered_map>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "net_interface.h"
#include "config.h"

namespace myframe {

class WsppServerImpl final : public NetInterface {
#ifdef WSPP_SERVER_USE_SSL
  enum tls_mode {
    MOZILLA_INTERMEDIATE = 1,
    MOZILLA_MODERN = 2
  };
  typedef websocketpp::server<websocketpp::config::asio_tls> server;
  typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;
#else
  typedef websocketpp::server<websocketpp::config::asio> server;
#endif
  typedef websocketpp::config::asio::message_type::ptr message_ptr;

 public:
  int SetOption(NetOption opt, const std::string& val) override;

  int Connect() override;

  int Send(void* dst, const std::string& data) override;

  int SetRecvHandle(recv_handle) override;

  int SetRecvHttpHandle(recv_http_handle) override;

  int SetRecvHttpHandle2(recv_http_handle2) override;

 private:
  void EventLoop();
  void OnHttp(server* s, websocketpp::connection_hdl hdl);
  void OnMessage(
    server* s,
    websocketpp::connection_hdl hdl,
    message_ptr msg);
  void OnClose(websocketpp::connection_hdl);
  void OnOpen(websocketpp::connection_hdl);
#ifdef WSPP_SERVER_USE_SSL
  context_ptr OnTlsInit(tls_mode mode, websocketpp::connection_hdl hdl);
  tls_mode mode_;
#endif
  std::string BuildJsonReq(server::connection_ptr con);
  std::shared_ptr<HttpReq> BuildPbReq(server::connection_ptr con);
  std::vector<std::string> Split(const std::string& str, char delim);
  bool ParseResource(const std::string& s, std::shared_ptr<HttpReq> req);

  std::string ca_;
  std::string key_;
  int port_;

  recv_handle recv_handle_;
  recv_http_handle recv_http_handle_;
  recv_http_handle2 recv_http_handle2_;
  std::unordered_map<void*, websocketpp::connection_hdl> conn_clis_;

  server srv_;
  std::thread th_;
};

}  // namespace myframe
