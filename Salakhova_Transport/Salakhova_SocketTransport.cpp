#include "pch.h"
#include "Salakhova_SocketTransport.h"

using namespace boost::asio;

SocketTransport::SocketTransport(ip::tcp::socket& s, std::mutex& mx)
    : sock(s), writeMx(mx) {
}

void SocketTransport::send(Message& m) const
{
    // ��������� �������, ����� ��� ������ �� ������ ������ � ����� ������������
    std::lock_guard<std::mutex> lg(writeMx);
    boost::system::error_code ec;

    write(sock, buffer(&m.header, sizeof(MessageHeader)), ec);
    if (ec) throw boost::system::system_error(ec);

    if (m.header.size > 0 && !m.data.empty())
    {
        write(sock, buffer(m.data.c_str(), m.header.size), ec);
        if (ec) throw boost::system::system_error(ec);
    }
}

void SocketTransport::receive(Message& m) const
{
    boost::system::error_code ec;

    read(sock, buffer(&m.header, sizeof(MessageHeader)), ec);
    if (ec) throw boost::system::system_error(ec);

    if (m.header.size > 0)
    {
        // �������� ������ ��� ������ (������ � ������ ����� �� ������ wchar_t)
        m.data.resize(m.header.size / sizeof(wchar_t));
        read(sock, buffer(&m.data[0], m.header.size), ec);
        if (ec) throw boost::system::system_error(ec);
    }
    else
    {
        m.data.clear();
    }
}
