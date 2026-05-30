#include "Salakhova_SocketTransport.h"
#include "Salakhova_SysProgh.h"

namespace ba = boost::asio;

SocketTransport::SocketTransport(boost::asio::ip::tcp::socket& s, std::mutex& mx)
    : sock(s), writeMx(mx) {}

void SocketTransport::send(Message& m) const
{
    SafeWrite("SocketTransport::send: messageType", m.header.messageType, "to", m.header.to, "from", m.header.from, "size", m.header.size);

    std::lock_guard<std::mutex> lg(writeMx);

    boost::system::error_code ec;
    ba::write(sock, ba::buffer(&m.header, sizeof(MessageHeader)), ec);
    if (ec) throw boost::system::system_error(ec);

    if (m.header.size > 0 && !m.data.empty())
    {
        ba::write(sock, ba::buffer(m.data.data(), m.header.size), ec);
        if (ec) throw boost::system::system_error(ec);
    }
}

void SocketTransport::receive(Message& m) const
{
    boost::system::error_code ec;

    ba::read(sock, ba::buffer(&m.header, sizeof(MessageHeader)), ec);
    if (ec) throw boost::system::system_error(ec);

    if (m.header.size > 0)
    {
        m.data.resize(m.header.size / sizeof(wchar_t));
        ba::read(sock, ba::buffer(&m.data[0], m.header.size), ec);
        if (ec) throw boost::system::system_error(ec);
    }
    else
    {
        m.data.clear();
    }
}
