/****************************************************************************
Copyright (c) 2018, likepeng
All rights reserved.

Author: likepeng <likepeng0418@163.com>
****************************************************************************/
#include "web_service_impl.h"

#include <glog/logging.h>
#include <jsoncpp/json/json.h>

namespace myframe {

int WebServiceImpl::SetOption(NetOption opt, const std::string& val) {
  switch (opt) {
  case NetOption::kServerPort:
    port_ = std::stoi(val);
    break;
  case NetOption::kServerCa:
    ca_ = val;
    break;
  case NetOption::kServerKey:
    key_ = val;
    break;
  default:
    LOG(ERROR) << "Unknown web option";
    return -1;
  }
  return 0;
}

int WebServiceImpl::Connect() {
  // Initialize ASIO
  srv_.init_asio();

  // Register our message handler
  using std::placeholders::_1;
  using std::placeholders::_2;
  srv_.set_open_handler(
    std::bind(&WebServiceImpl::OnOpen, this, _1));
  srv_.set_close_handler(
    std::bind(&WebServiceImpl::OnClose, this, _1));
  srv_.set_message_handler(
    std::bind(&WebServiceImpl::OnMessage, this, &srv_, _1, _2));
  srv_.set_http_handler(
    std::bind(&WebServiceImpl::OnHttp, this, &srv_, _1));
#ifdef USE_SSL
  srv_.set_tls_init_handler(
    std::bind(&WebServiceImpl::OnTlsInit, this, MOZILLA_INTERMEDIATE, _1));
#endif

  srv_.set_reuse_addr(true);
  
  try {
    // Listen on port
    srv_.listen(port_);
    // Start the server accept loop
    srv_.start_accept();
  } catch (std::exception& e) {
    LOG(ERROR) << e.what();
    return -1;
  }

  th_ = std::thread(std::bind(&WebServiceImpl::EventLoop, this));
  th_.detach();
  return 0;
}

int WebServiceImpl::Send(void* dst, const std::string& data) {
  if (conn_clis_.find(dst) == conn_clis_.end()) {
    LOG(ERROR) << "can't find conn";
    return -1;
  }
  auto h = conn_clis_[dst];
  if (h.lock() == nullptr) {
    LOG(ERROR) << "conn is closed";
    return -1;
  }
  auto conn = srv_.get_con_from_hdl(h);
  auto ec = conn->send(data, websocketpp::frame::opcode::binary);
  if (ec) {
    LOG(ERROR) << "send error, " << ec.message();
    return -1;
  }
  return 0;
}

int WebServiceImpl::SetRecvHandle(recv_handle h) {
  recv_handle_ = h;
  return 0;
}

int WebServiceImpl::SetRecvHttpHandle(recv_http_handle h) {
  recv_http_handle_ = h;
  return 0;
}

int WebServiceImpl::SetRecvHttpHandle2(recv_http_handle2 h) {
  recv_http_handle2_ = h;
  return 0;
}

void WebServiceImpl::EventLoop() {
  // Start the ASIO io_service run loop
  srv_.run();
  LOG(INFO) << "websockets service exit";
}

/*
req:
{
  "uri":""
  "method":"GET",
  "header":{
    "":""
  },
  "body":""
}
rep:
{
  "code":404
  "body":""
}
*/
std::string WebServiceImpl::BuildJsonReq(server::connection_ptr con) {
  Json::Value v;
  v["uri"] = con->get_request().get_uri();
  v["method"] = con->get_request().get_method();
  auto headers = con->get_request().get_headers();
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    v["header"][it->first] = it->second;
  }
  v["body"] = con->get_request().get_body();
  return v.toStyledString();
}

std::vector<std::string> WebServiceImpl::Split(const std::string& str, char delim) {
  std::vector<std::string> name_list;
  std::string item;
  std::stringstream ss(str);
  while (std::getline(ss, item, delim)) {
    name_list.push_back(item);
  }
  return name_list;
}

bool WebServiceImpl::ParseResource(const std::string& s, std::shared_ptr<HttpReq> req) {
  auto pos = s.find('?');
  if (pos == s.npos) {
    // TODO check path is ok
    req->set_path(s);
    return true;
  }
  // TODO check path is ok
  req->set_path(s.substr(0, pos));
  auto params_str = s.substr(pos + 1);
  // spilt params
  auto param_pair = Split(params_str, '&');
  for (int i = 0; i < param_pair.size(); ++i) {
    auto pair = Split(param_pair[i], '=');
    if (pair.size() != 2) {
      LOG(ERROR) << "param fmt error";
      return false;
    }
    auto param = req->add_params();
    param->set_key(pair[0]);
    param->set_value(pair[1]);
  }
  return true;
}

std::shared_ptr<HttpReq> WebServiceImpl::BuildPbReq(server::connection_ptr con) {
  auto req = std::make_shared<HttpReq>();
  DLOG(INFO) << "http req: " << con->get_request().raw();
  req->set_uri(con->get_request().get_uri());
  ParseResource(con->get_request().get_uri(), req);
  req->set_method(con->get_request().get_method());
  auto headers = con->get_request().get_headers();
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    auto header = req->add_headers();
    header->set_key(it->first);
    header->set_value(it->second);
  }
  req->set_body(con->get_request().get_body());
  return req;
}

void WebServiceImpl::OnHttp(server* s, websocketpp::connection_hdl hdl) {
  server::connection_ptr con = s->get_con_from_hdl(hdl);

  auto err_resp_fun = [con](int line){
    con->set_body("");
    con->set_status(websocketpp::http::status_code::ok);
    DLOG(INFO)
      << "line " << line;
    DLOG(INFO)
      << "unknown http req: " << con->get_request().raw();
  };

  if (recv_http_handle_) {
    auto req_str = BuildJsonReq(con);
    std::string resp_str;
    resp_str = recv_http_handle_(req_str);
    if (resp_str.empty()) {
      err_resp_fun(__LINE__);
      return;
    }
    Json::Value v;
    Json::Reader reader;
    if (!reader.parse(resp_str, v)) {
      err_resp_fun(__LINE__);
      return;
    }
    con->set_body(v["body"].asString());
    con->set_status((websocketpp::http::status_code::value)(v["code"].asInt()));
  } else if (recv_http_handle2_) {
    auto req_pb = BuildPbReq(con);
    auto resp_pb = recv_http_handle2_(req_pb);
    if (resp_pb == nullptr) {
      err_resp_fun(__LINE__);
      return;
    }
    con->set_body(resp_pb->body());
    con->set_status((websocketpp::http::status_code::value)(resp_pb->code()));
  } else {
    err_resp_fun(__LINE__);
  }
}

void WebServiceImpl::OnMessage(
  server* s,
  websocketpp::connection_hdl hdl,
  message_ptr msg) {
  DLOG(INFO)
    << "on_message called with hdl: " << hdl.lock().get()
    << " and message: " << msg->get_payload();
  try {
    if (recv_handle_) {
      recv_handle_(hdl.lock().get(), msg->get_payload());
    }
  } catch (websocketpp::exception const & e) {
    LOG(ERROR)
      << "Echo failed because: "
      << "(" << e.what() << ")";
  }
}

void WebServiceImpl::OnClose(websocketpp::connection_hdl h) {
  if (conn_clis_.find(h.lock().get()) == conn_clis_.end()) {
    LOG(ERROR) << "conn is already erased";
    return;
  }
  conn_clis_.erase(h.lock().get());
  LOG(INFO) << "conn closed " << h.lock().get();
}

void WebServiceImpl::OnOpen(websocketpp::connection_hdl h) {
  if (conn_clis_.find(h.lock().get()) != conn_clis_.end()) {
    LOG(ERROR) << "conn is already in conn map";
    return;
  }
  conn_clis_[h.lock().get()] = h;
  LOG(INFO) << "new conn " << h.lock().get();
}

#ifdef USE_SSL
WebServiceImpl::context_ptr WebServiceImpl::OnTlsInit(tls_mode mode, websocketpp::connection_hdl hdl) {
  namespace asio = websocketpp::lib::asio;

  LOG(INFO) << "on_tls_init called with hdl: " << hdl.lock().get();
  LOG(INFO) << "using TLS mode: " << (mode == MOZILLA_MODERN ? "Mozilla Modern" : "Mozilla Intermediate");

  context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

  try {
    if (mode == MOZILLA_MODERN) {
      // Modern disables TLSv1
      ctx->set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::no_tlsv1 |
        asio::ssl::context::single_dh_use);
    } else {
      ctx->set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::single_dh_use);
    }
    // ctx->set_password_callback(bind(&get_password));
    ctx->use_certificate_chain_file(ca_);
    ctx->use_private_key_file(key_, asio::ssl::context::pem);
    
    // Example method of generating this file:
    // `openssl dhparam -out dh.pem 2048`
    // Mozilla Intermediate suggests 1024 as the minimum size to use
    // Mozilla Modern suggests 2048 as the minimum size to use.
    // ctx->use_tmp_dh_file("dh.pem");
    
    // std::string ciphers;
    
    // if (mode == MOZILLA_MODERN) {
    //   ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
    // } else {
    //   ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
    // }
    
    // if (SSL_CTX_set_cipher_list(ctx->native_handle() , ciphers.c_str()) != 1) {
    //   LOG(ERROR) << "Error setting cipher list";
    // }
  } catch (std::exception& e) {
    LOG(ERROR) << "Exception: " << e.what();
  }
  return ctx;
}
#endif

}  // namespace myframe