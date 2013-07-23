#ifndef OBELISK_CLIENT_INTERFACE
#define OBELISK_CLIENT_INTERFACE

#include <bitcoin/bitcoin.hpp>
#include <obelisk/client/blockchain.hpp>

class subscriber_part
{
public:
    typedef std::function<void (size_t, const bc::block_type&)>
        block_notify_callback;
    typedef std::function<void (const bc::transaction_type&)>
        transaction_notify_callback;

    subscriber_part(zmq::context_t& context);
    bool subscribe_blocks(const std::string& connection,
        block_notify_callback notify_block);
    bool subscribe_transactions(const std::string& connection,
        transaction_notify_callback notify_tx);
    void update();

private:
    typedef std::unique_ptr<zmq::socket_t> zmq_socket_uniqptr;

    bool setup_socket(const std::string& connection,
        zmq_socket_uniqptr& socket);

    void recv_tx();
    void recv_block();

    zmq::context_t& context_;
    zmq_socket_uniqptr socket_block_, socket_tx_;
    block_notify_callback notify_block_;
    transaction_notify_callback notify_tx_;
};

class fullnode_interface
{
public:
    fullnode_interface(const std::string& connection);
    bool subscribe_blocks(const std::string& connection,
        subscriber_part::block_notify_callback notify_block);
    bool subscribe_transactions(const std::string& connection,
        subscriber_part::transaction_notify_callback notify_tx);
    void update();
    void stop(const std::string& secret);
    blockchain_interface blockchain;

private:
    zmq::context_t context_;
    backend_cluster backend_;
    subscriber_part subscriber_;
};

#endif

