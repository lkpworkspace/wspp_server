/****************************************************************************
Copyright (c) 2023, likepeng
All rights reserved.

Author: likepeng <likepeng0418@163.com>
****************************************************************************/
#pragma once
#include <functional>
#include <string>
#include <mutex>

#include "myframe_pb/http.pb.h"

using recv_handle =
  std::function<void(void* handle, const std::string& data)>;

using recv_http_handle =
  std::function<std::string(const std::string& data)>;

using recv_http_handle2 =
  std::function<std::shared_ptr<myframe::HttpResp>(const std::shared_ptr<myframe::HttpReq>&)>;

namespace myframe {

enum class NetOption : int {
  kServerCa = 0,
  kServerKey = 1,
  kServerPort = 2,
};

/**
 * @brief 网络协议统一接口
 */
class NetInterface {
 public:
  NetInterface() = default;
  virtual ~NetInterface() {}

  /**
   * SetOption() - 设置网络参数
   * @opt:  见枚举 NetOption
   * @val:  值
   * 
   * @return: -1 设置失败, 0 设置成功
   */
  virtual int SetOption(NetOption opt, const std::string& val) = 0;

  /**
   * Connect() - 连接服务器，并启动事件循环
   * 
   * @return: -1 设置失败, 0 设置成功
   */
  virtual int Connect() = 0;

  /**
   * Send() - 发送数据
   * @dst:    mqtt可以填写要发送的topic，其它协议可以不填
   * @data:   数据
   * 
   * @return: -1 设置失败, 0 设置成功
   */
  virtual int Send(void* handle, const std::string& data) = 0;

  /**
   * SetRecvHandle() - 设置接收消息回调，设置后就不能通过Recv接收消息
   * @recv_handle: 回调函数
   * 
   * @return: -1 设置失败，0 设置成功
   */
  virtual int SetRecvHandle(recv_handle) {
    return -1;
  }
  virtual int SetRecvHttpHandle(recv_http_handle) {
    return -1;
  }
  virtual int SetRecvHttpHandle2(recv_http_handle2) {
    return -1;
  }

  /**
   * Recv() - 阻塞接收服务器数据
   * @src: mqtt返回topic，其它协议为空
   * @data: 数据
   * @return: -1 设置失败，0 设置成功
   */
  virtual int Recv(void* src, std::string* data) {
    return -1;
  }
};

}  // myframe
