//
// modem.cpp
//
// The TCP connection behind the R: device. The emulator asks for a call
// with host_modem_dial(); the modem task looks the name up, connects (both
// wait, so they are done here and not by the emulator), then moves bytes
// between the socket and two ring buffers, which the emulator fills and
// empties with host_modem_send() and host_modem_receive(). The task runs
// whenever the emulator yields to Circle's tasks, once a frame.
//
// Sources:
//  - Circle, include/circle/net/socket.h and lib/net/socket.cpp: CSocket;
//    Receive() needs a buffer of FRAME_BUFFER_SIZE or data may be lost, and
//    with MSG_DONTWAIT returns 0 when nothing has arrived;
//    lib/net/tcpconnection.cpp: Receive() and Send() return
//    -NET_ERROR_CONNECTION_RESET once the other end has closed.
//  - Circle, lib/net/dnsclient.cpp: CDNSClient::Resolve() takes names and
//    dotted addresses.
//  - src/network_time_sync.cpp: a network task started with the network.
//
// This file is part of BMA800XL, a bare metal Atari 800XL emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#include "modem.h"
#include "host.h"

#include <string.h>

#include <circle/logger.h>
#include <circle/net/dnsclient.h>
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/netdevice.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

static const char FromModem[] = "modem";

namespace {

// One writer, one reader. Both run on the same core, between yields.
template <unsigned Size> class Ring {
public:
  Ring() : m_head(0), m_tail(0) {}

  void Clear() { m_tail = m_head; }
  unsigned Used() const { return m_head - m_tail; }
  unsigned Free() const { return Size - Used(); }

  unsigned Put(const uint8_t *data, unsigned length) {
    unsigned n = 0;
    while (n < length && Used() < Size) {
      m_data[m_head % Size] = data[n++];
      m_head = m_head + 1;
    }
    return n;
  }

  unsigned Get(uint8_t *data, unsigned size) {
    unsigned n = 0;
    while (n < size && Used() > 0) {
      data[n++] = m_data[m_tail % Size];
      m_tail = m_tail + 1;
    }
    return n;
  }

private:
  uint8_t m_data[Size];
  volatile unsigned m_head;
  volatile unsigned m_tail;
};

class CModemTask : public CTask {
public:
  explicit CModemTask(CNetSubSystem *network)
      : m_network(network), m_socket(0), m_socket_call(0),
        m_state(HOST_MODEM_IDLE),
        m_call(0), m_dial(false), m_port(23) {
    m_host[0] = '\0';
    SetName("modem");
  }

  bool Dial(const char *host, unsigned port) {
    if (!m_network->IsRunning() ||
        m_network->GetConfig()->GetIPAddress()->IsNull()) {
      return false;
    }
    strncpy(m_host, host, sizeof m_host - 1);
    m_host[sizeof m_host - 1] = '\0';
    m_port = port;
    m_call = m_call + 1;
    m_dial = true;
    m_state = HOST_MODEM_DIALLING;
    m_tx.Clear();
    m_rx.Clear();
    return true;
  }

  void HangUp() {
    m_call = m_call + 1;
    m_dial = false;
    m_state = HOST_MODEM_IDLE;
    m_tx.Clear();
    m_rx.Clear();
  }

  int State() const { return m_state; }
  unsigned Send(const uint8_t *data, unsigned length) {
    return m_state == HOST_MODEM_CONNECTED ? m_tx.Put(data, length) : 0;
  }
  unsigned Receive(uint8_t *data, unsigned size) {
    return m_rx.Get(data, size);
  }

  void Run(void) override {
    for (;;) {
      // A hang up or a new call ends the call in progress.
      if (m_socket != 0 && m_call != m_socket_call) {
        Close();
      }
      if (m_dial) {
        m_dial = false;
        Connect();
      }
      if (m_socket != 0) {
        Transfer();
      }
      CScheduler::Get()->MsSleep(m_socket != 0 ? 1 : 10);
    }
  }

private:
  void Connect() {
    unsigned call = m_call;
    char host[sizeof m_host];
    strcpy(host, m_host);
    unsigned port = m_port;

    CLogger::Get()->Write(FromModem, LogNotice, "Dialling %s port %u", host,
                          port);
    CIPAddress address;
    CDNSClient dns(m_network);
    if (!dns.Resolve(host, &address)) {
      CLogger::Get()->Write(FromModem, LogWarning, "Cannot find %s", host);
      Failed(call);
      return;
    }
    if (call != m_call) {
      return;
    }

    CSocket *socket = new CSocket(m_network, IPPROTO_TCP);
    if (socket->Connect(address, (u16)port) < 0) {
      CLogger::Get()->Write(FromModem, LogWarning, "Cannot connect to %s",
                            host);
      delete socket;
      Failed(call);
      return;
    }
    if (call != m_call) {
      delete socket;
      return;
    }
    CLogger::Get()->Write(FromModem, LogNotice, "Connected to %s", host);
    m_socket = socket;
    m_socket_call = call;
    m_state = HOST_MODEM_CONNECTED;
  }

  void Failed(unsigned call) {
    if (call == m_call) {
      m_state = HOST_MODEM_IDLE;
    }
  }

  void Close() {
    delete m_socket;
    m_socket = 0;
  }

  void Transfer() {
    while (m_rx.Free() >= sizeof m_buffer) {
      int n = m_socket->Receive(m_buffer, sizeof m_buffer, MSG_DONTWAIT);
      if (n < 0) {
        Disconnected();
        return;
      }
      if (n == 0) {
        break;
      }
      m_rx.Put(m_buffer, (unsigned)n);
    }
    while (m_tx.Used() > 0) {
      unsigned n = m_tx.Get(m_buffer, sizeof m_buffer);
      if (m_socket->Send(m_buffer, n, MSG_DONTWAIT) < 0) {
        Disconnected();
        return;
      }
    }
  }

  void Disconnected() {
    CLogger::Get()->Write(FromModem, LogNotice, "Call ended");
    Close();
    m_state = HOST_MODEM_IDLE;
  }

  CNetSubSystem *m_network;
  CSocket *m_socket;
  unsigned m_socket_call;
  volatile int m_state;
  // Counts calls and hang ups, so a call that ended while it was being
  // set up is dropped.
  volatile unsigned m_call;
  volatile bool m_dial;
  char m_host[128];
  unsigned m_port;
  Ring<16384> m_rx;
  Ring<4096> m_tx;
  uint8_t m_buffer[FRAME_BUFFER_SIZE];
};

CModemTask *s_modem = 0;

}  // namespace

void StartModem(CNetSubSystem *network) { s_modem = new CModemTask(network); }

extern "C" {

int host_modem_dial(const char *host, unsigned port) {
  return s_modem != 0 && s_modem->Dial(host, port);
}

void host_modem_hang_up(void) {
  if (s_modem != 0) {
    s_modem->HangUp();
  }
}

int host_modem_state(void) {
  return s_modem != 0 ? s_modem->State() : HOST_MODEM_IDLE;
}

unsigned host_modem_send(const uint8_t *data, unsigned length) {
  return s_modem != 0 ? s_modem->Send(data, length) : 0;
}

unsigned host_modem_receive(uint8_t *data, unsigned size) {
  return s_modem != 0 ? s_modem->Receive(data, size) : 0;
}

}  // extern "C"
