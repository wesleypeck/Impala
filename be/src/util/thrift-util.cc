// Copyright (c) 2012 Cloudera, Inc. All rights reserved.

#include "util/thrift-util.h"

#include <Thrift.h>
#include <transport/TSocket.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <server/TNonblockingServer.h>
#include <transport/TServerSocket.h>
#include <concurrency/ThreadManager.h>
#include <concurrency/PosixThreadFactory.h>

#include "util/hash-util.h"
#include "util/thrift-server.h"
#include "gen-cpp/Types_types.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;
using namespace apache::thrift::protocol;
using namespace apache::thrift::concurrency;
using namespace boost;

namespace impala {

std::size_t hash_value(const THostPort& host_port) {
  uint32_t hash = HashUtil::Hash(host_port.host.c_str(), host_port.host.length(), 0);
  return HashUtil::Hash(&host_port.port, sizeof(host_port.port), hash);
}

// Comparator for THostPorts. Thrift declares this (in gen-cpp/Types_types.h) but
// never defines it.
bool THostPort::operator<(const THostPort& that) const {
  if (this->host < that.host) {
    return true;
  } else if ((this->host == that.host) && (this->port < that.port)) {
    return true;
  }
  return false;
};

static void ThriftOutputFunction(const char* output) {
  VLOG_QUERY << output;
}

void InitThriftLogging() {
  GlobalOutput.setOutputFunction(ThriftOutputFunction);
}

Status WaitForLocalServer(const ThriftServer& server, int num_retries,
    int retry_interval_ms) {
  return WaitForServer("localhost", server.port(), num_retries, retry_interval_ms);
}

Status WaitForServer(const string& host, int port, int num_retries,
    int retry_interval_ms) {
  int retry_count = 0;
  while (retry_count < num_retries) {
    try {
      TSocket socket(host, port);
      // Timeout is in ms
      socket.setConnTimeout(500);
      socket.open();
      socket.close();
      return Status::OK;
    } catch (TTransportException& e) {
      VLOG_QUERY << "Connection failed: " << e.what();
    }
    ++retry_count;
    VLOG_QUERY << "Waiting " << retry_interval_ms << "ms for Thrift server at "
               << host << ":" << port << " to come up, failed attempt " << retry_count
               << " of " << num_retries + 1;
    usleep(retry_interval_ms * 1000);
  }
  return Status("Server did not come up");
}

}